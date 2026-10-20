// Compare actual ISO-backed decoding with the owned-memory path using private
// original game installations. This executable does not create a game window.
#include "../src/media.h"
#include "../src/sys.h"
#include "../src/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <array>
#include <map>
#include <vector>

struct Read { uint64_t begin,end; };
static std::map<void *,size_t> allocations;
static std::vector<Read> reads;
static int capture_reads,edition,failures,checks,cases;
static char context[256];
static void check(bool condition,const char *message){
  checks++;if(!condition){failures++;printf("FAIL [%s]: %s\n",context,message);}
}
void *sys_alloc(size_t n){void *p=calloc(1,n?n:1);if(p)allocations[p]=n;return p;}
void *sys_realloc(void *p,size_t n){allocations.erase(p);void *q=realloc(p,n?n:1);if(q)allocations[q]=n;return q;}
void sys_free(void *p){allocations.erase(p);free(p);}
void *sys_fopen(const char *path){return fopen(path,"rb");}
int sys_fread_at(void *file,uint64_t offset,void *buffer,uint32_t count){
  if(capture_reads)reads.push_back({offset,offset+count});
#ifdef _WIN32
  return _fseeki64((FILE *)file,(__int64)offset,SEEK_SET)==0&&fread(buffer,1,count,(FILE *)file)==count;
#else
  return fseeko((FILE *)file,(off_t)offset,SEEK_SET)==0&&fread(buffer,1,count,(FILE *)file)==count;
#endif
}
void sys_fclose(void *file){fclose((FILE *)file);}
uint8_t *sys_read_file(const char *path,size_t *size){
  FILE *file=fopen(path,"rb");if(!file)return 0;fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);
  if(length<0){fclose(file);return 0;}uint8_t *data=(uint8_t *)sys_alloc((size_t)length+1);
  *size=fread(data,1,(size_t)length,file);fclose(file);return data;
}
void sys_log(const char *message){fprintf(stderr,"%s\n",message);}
int sys_debug_enabled(){return 0;}
int sys_game_language(){return edition;}
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static std::array<char,65> image_hash(const Video &v){
  std::array<char,65> result;Sha256 sha;sha.init();sha.update(v.rgba,(size_t)v.w*v.h*4);sha.finish(result.data());return result;
}
static size_t payload_ranges(const uint8_t *data,size_t size,std::vector<Read> &iv,std::vector<Read> &sd){
  size_t largest=0,offset=size>=64&&!memcmp(data,"UBB2",4)?64:0;
  while(offset+8<=size){
    uint32_t bytes=rd32(data+offset)&0xffffff;if(bytes<4||bytes>size-offset)break;
    largest=std::max(largest,(size_t)bytes);
    for(size_t chunk=offset+4;chunk+8<=offset+bytes;){
      uint32_t count=rd32(data+chunk);if(count<8||count>offset+bytes-chunk)break;
      if(data[chunk+4]=='I'&&data[chunk+5]=='V')iv.push_back({chunk+8,chunk+count});
      if(data[chunk+4]=='S'&&data[chunk+5]=='D')sd.push_back({chunk+8,chunk+count});
      chunk+=(count+3)&~3u;
    }
    offset+=bytes;
  }
  return largest;
}
static bool touches_payload(const Video &v,const std::vector<Read> &payloads){
  for(const Read &read:reads){
    if(read.end<=v.stream_offset||read.begin>=v.stream_offset+v.size)continue;
    uint64_t begin=read.begin>=v.stream_offset?read.begin-v.stream_offset:0;
    uint64_t end=read.end-v.stream_offset;
    auto payload=std::lower_bound(payloads.begin(),payloads.end(),begin,[](const Read &range,uint64_t position){return range.end<=position;});
    if(payload!=payloads.end()&&payload->begin<end){
      printf("Index read overlaps compressed payload: relative_read=[%llu,%llu), payload=[%llu,%llu)\n",
             (unsigned long long)begin,(unsigned long long)end,(unsigned long long)payload->begin,(unsigned long long)payload->end);
      return true;
    }
  }
  return false;
}

int main(int argc,char **argv){
  if(argc!=2){printf("usage: video_streaming WORKSPACE_ROOT\n");return 2;}
  const char *languages[]={"English","French","German"};
  const char *videos[]={"TRACK1/FACTO.UBB","TRACK1/TIBET.UBB","TRACK1/EXTRA.UBB","TRACK2/LUNAR.UBB","TRACK2/BAYOU.UBB","TRACK2/BRONX.UBB",
                        "BOYLE/INTRO25.UBB","BOYLE/BOYLE01.UBB","BOYLE/BOYLE06.UBB"};
  std::array<char,65> track_hashes[6][12];
  for(edition=0;edition<3;edition++){
    char directory[1024],path[1200];snprintf(directory,sizeof directory,"%s/Megarace 2 %s",argv[1],languages[edition]);
    snprintf(context,sizeof context,"%s fixture",languages[edition]);
    snprintf(path,sizeof path,"%s/MEGARACE2.gog",directory);if(!iso_open(path)){check(false,"private original ISO opens");continue;}
    snprintf(path,sizeof path,"%s/M.EXE",directory);if(!mexe_load(path)){check(false,"private original decoder tables load");continue;}
    for(int clip=0;clip<9;clip++){
      snprintf(context,sizeof context,"%s %s",languages[edition],videos[clip]);
      size_t size=0;uint8_t *data=iso_load(videos[clip],&size);check(data!=0,"reference source is available");if(!data)continue;
      std::vector<Read> iv,sd;size_t largest=payload_ranges(data,size,iv,sd);check(!iv.empty(),"independent parser finds compressed IV payload ranges");
      Video memory={},stream={};int audio=clip>=6;
      if(!video_open(&memory,data,size,clip<6?350:400,audio)){check(false,"memory reference opens");continue;}
      reads.clear();capture_reads=1;int opened=video_open_iso(&stream,videos[clip],clip<6?350:400,audio);capture_reads=0;
      check(opened!=0,"ISO-backed stream opens");if(!opened){video_close(&memory);continue;}
      cases++;
      check(stream.streaming&&stream.data&&allocations[stream.data]==1,"stream owns an identity token instead of the complete compressed file");
      check(stream.stream_capacity<=largest+64&&stream.stream_capacity<size,"index scratch is bounded by a compressed chunk");
      check(!touches_payload(stream,iv),"index reads headers/palettes/audio without fetching any IV payload bytes");
      check(stream.stream_bytes_read<size,"index does not read the whole compressed source");
      check(stream.w==memory.w&&stream.h==memory.h&&stream.nframes==memory.nframes&&stream.npals==memory.npals,
            "stream dimensions/frame count/palette count match memory reference");
      check(stream.fps==memory.fps&&stream.fps_exact==memory.fps_exact,"stream keeps exact source cadence");
      check(!memcmp(stream.frame_ofs,memory.frame_ofs,memory.nframes*sizeof(uint32_t)),"frame offsets match memory reference");
      check(!memcmp(stream.keyframe,memory.keyframe,memory.nframes),"keyframe flags match memory reference");
      check(!memcmp(stream.pal_id,memory.pal_id,memory.nframes*sizeof(uint16_t)),"per-frame palette assignment matches memory reference");
      check(!memcmp(stream.pals,memory.pals,memory.npals*768),"every indexed palette byte matches memory reference");
      check(stream.pcm_frames==memory.pcm_frames&&((!audio&&!stream.pcm)||
            (audio&&stream.pcm&&memory.pcm&&!memcmp(stream.pcm,memory.pcm,(size_t)memory.pcm_frames*4))),
            "cinematic PCM is exact and muted track streams allocate no PCM");
      if(audio){
        Video muted={};reads.clear();capture_reads=1;int silent=video_open_iso(&muted,videos[clip],400,0);capture_reads=0;
        check(silent&&!muted.pcm&&!muted.pcm_frames,"muted cinematic stream skips PCM allocation and decoding");
        check(silent&&muted.nframes==memory.nframes&&muted.fps_exact==memory.fps_exact,"muted cinematic retains its original frame count and exact cadence");
        check(silent&&!touches_payload(muted,iv)&&!touches_payload(muted,sd),"muted cinematic indexing reads neither video nor sound payload bytes");
        video_close(&muted);
      }
      int frames[12]={0,1,2,24,150,30,memory.nframes-1,0,memory.nframes/2,memory.nframes/2,memory.nframes-2,memory.nframes-1};
      std::array<char,65> expected[12];
      for(int sample=0;sample<12;sample++){
        check(video_decode(&memory,frames[sample])!=0,"memory reference decodes the forward/backward/repeat seek sequence");
        expected[sample]=image_hash(memory);
      }
      uint64_t index_bytes=stream.stream_bytes_read;
      for(int sample=0;sample<12;sample++){
        uint64_t before=stream.stream_bytes_read;
        check(video_decode(&stream,frames[sample])!=0,"ISO-backed stream decodes the forward/backward/repeat seek sequence");
        std::array<char,65> actual=image_hash(stream);check(actual==expected[sample],"decoded RGBA bytes match the full-memory path");
        if(sample==9)check(stream.stream_bytes_read==before,"repeated frame reuses decoded pixels without disk reads");
        if(clip<6){if(!edition)track_hashes[clip][sample]=actual;else check(actual==track_hashes[clip][sample],"track RGBA remains identical across English/French/German editions");}
      }
      check(stream.stream_bytes_read>index_bytes,"playback fetches compressed frames on demand after indexing");
      check(stream.stream_capacity<=largest+64&&stream.stream_capacity<size,"forward/backward playback retains only a bounded compressed-chunk buffer");
      uint64_t decoded_bytes=stream.stream_bytes_read;
      video_close(&stream);video_close(&memory);
      check(!stream.data&&!stream.stream_buffer&&!stream.pcm&&!stream.rgba&&!stream.nframes,"closing releases stream ownership and decoder outputs");
      printf("CASE %s: source=%zu, largest_chunk=%zu, index_reads=%llu, playback_reads=%llu\n",context,size,largest,
             (unsigned long long)index_bytes,(unsigned long long)(decoded_bytes-index_bytes));fflush(stdout);
    }
  }
  printf("Video streaming: %d cases, %d checks, %d failures\n",cases,checks,failures);return failures?1:0;
}
