// Retro-only menus: retain the 0.7.6 layout, native bitmap sizes and 640x480 pixels.
static void retro_menu_open(int state){setup_save();G.state=state;G.sel=0;G.t=0;G.pad_wait=-1;G.menu_click=-1;g_audio_drag=-1;}
static void screen_retro_options(const Input *in){
  cv_pic(G.bg,0,0,1);cv_text(g_font_h14,330,6,"MEGARACE II SETUP",ORANGE,1,1);
  struct Item{int x,y,group,val;const char *txt;};
  static const Item items[]={
    {73,71,0,0,"Revived"},{73,100,0,1,"Retro Mode"},{73,129,9,0,"Erweiterte\nGrafik ..."},
    {333,71,10,0,"Advanced sound"},
    {73,212,3,0,"Nein"},{73,241,3,1,"Ja"},
    {333,212,4,0,"Original (5/5/3)"},{333,241,4,3,"3 Runden"},{333,270,4,5,"5 Runden"},
    {73,351,7,0,"Retro controller"},{333,351,8,0,"Debugmen\xfc"},{73,397,6,0,"Tasten"}};
  cv_text(g_font_h14,17,40,"Grafikmodi",ORANGE,0,1);cv_text(g_font_h14,318,40,"Ger\xe4usche",ORANGE,0,1);
  cv_text(g_font_h14,17,180,"Lance Boyle",ORANGE,0,1);cv_text(g_font_h14,318,180,"Rennen",ORANGE,0,1);
  cv_text(g_font_h14,17,321,"Steuerung",ORANGE,0,1);
  cv_text(g_font_small,630,326,g_pad.connected?"(Controller verbunden)":"(kein Controller gefunden)",g_pad.connected?GREEN:GREY,2,1);
  const int n=sizeof items/sizeof items[0];if(G.sel<0||G.sel>n)G.sel=0;int hover=-1;
  for(int i=0;i<n;i++){
    const Item &e=items[i];int on=e.group==0?G.gfx_mode==e.val:e.group==3?G.opts_clips==e.val:e.group==4?G.opts_laps==e.val:0;
    const SprFile &sp=on?G.chk_on:G.chk_off;if(sp.n)cv_spr(spr_at(sp.f[0],e.x-3,e.y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h14,e.x+28,e.y,e.txt,G.sel==i&&G.kbd?WHITE:BEIGE,0,1);
    if(hit(e.x-3,e.y-4,30+cv_text_w(g_font_h14,e.txt,1),e.group==9?48:26))hover=i;
  }
  if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],443,441),G.bg.pal,0,0,1);
  cv_text(g_font_h14,496,443,"\xdc" "bernehmen",G.sel==n&&G.kbd?WHITE:BEIGE,0,1);if(hit(440,436,200,30))hover=n;
  if(in->pressed[K_DOWN]||in->pressed[K_RIGHT]){G.sel=(G.sel+1)%(n+1);G.kbd=1;}
  if(in->pressed[K_UP]||in->pressed[K_LEFT]){G.sel=(G.sel+n)%(n+1);G.kbd=1;}
  if(in->mouse_dx||in->mouse_dy)G.kbd=0;
  int act=G.click?hover:(in->pressed[K_ENTER]||in->pressed[K_SPACE])?G.sel:-1;if(in->pressed[K_ESC])act=n;
  if(act>=0){
    msfx(S_MENU2,0.7f);if(act==n){setup_save();options_close();return;}
    const Item &e=items[act];
    if(e.group==0){graphics_mode_set(e.val);G.sel=e.val;}
    if(e.group==3)G.opts_clips=e.val;if(e.group==4)G.opts_laps=e.val;
    if(e.group==9){retro_menu_open(GS_GFX);return;}
    if(e.group==10){retro_menu_open(GS_RETRO_SOUND);return;}
    if(e.group==7){retro_menu_open(GS_PADMAP);return;}
    if(e.group==8){retro_menu_open(GS_DEBUG);return;}
    if(e.group==6)show_message("Tasten im Rennen:\nPfeil hoch: Gas     Pfeil runter: Bremse\nLinks / Rechts: lenken\nLeertaste: Frontwaffe   Tab: wechseln\nEingabe: Heckwaffe   /: wechseln\nEsc: Pause",GS_OPTIONS);
    setup_apply();
  }
  draw_cursor();
}
static void retro_volume_bar(int x,int y,int value,uint32_t color){
  // 25 coarse cells, hard edges, two-pixel bevels and a block cursor; no filtering.
  cv_rect(x-2,y-2,504,20,rgba(30,27,22,255));
  for(int i=0;i<25;i++){
    int xx=x+i*20;uint32_t fill=i*4<value?ORANGE:rgba(78,72,57,255);
    cv_rect(xx,y,18,16,fill);cv_rect(xx,y,18,2,rgba(180,165,130,255));cv_rect(xx+16,y+2,2,14,rgba(22,20,17,255));
  }
  int cursor=x+(value*492/100)/4*4;cv_rect(cursor,y-4,8,24,color);cv_rect(cursor+2,y-2,4,20,rgba(128,115,85,255));
}
static void screen_retro_sound(const Input *in){
  cv_pic(G.bg,0,0,1);cv_text(g_font_h14,320,6,"Advanced sound",ORANGE,1,1);
  static const char *labels[]={"Sounds","Engines","Race music","Menu music","Menu sounds"};int hover=-1;
  if(G.sel<0||G.sel>5)G.sel=0;
  for(int b=0;b<AUDIO_COUNT;b++){
    int y=56+b*66;uint32_t col=G.kbd&&G.sel==b?WHITE:BEIGE;
    cv_text(g_font_h14,70,y,labels[b],col,0,1);char percent[16]="";str_int(percent,sizeof percent,g_audio_percent[b]);str_cat(percent,"%",sizeof percent);cv_text(g_font_h14,570,y,percent,col,2,1);
    retro_volume_bar(70,y+28,g_audio_percent[b],col);if(hit(66,y-3,508,54))hover=b;
  }
  cv_text(g_font_small,320,395,"Audio off hint",GREY,1,1);cv_text(g_font_small,320,416,"Volume controls hint",GREY,1,1);
  if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],443,441),G.bg.pal,0,0,1);
  cv_text(g_font_h14,496,443,"\xdc" "bernehmen",G.kbd&&G.sel==5?WHITE:BEIGE,0,1);if(hit(440,436,200,30))hover=5;
  if(in->pressed[K_DOWN]){G.sel=(G.sel+1)%6;G.kbd=1;}if(in->pressed[K_UP]){G.sel=(G.sel+5)%6;G.kbd=1;}
  int dir=in->pressed[K_RIGHT]?1:in->pressed[K_LEFT]?-1:0;if(dir&&G.sel<5){G.kbd=1;audio_volume_set(G.sel,g_audio_percent[G.sel]+dir*5);}
  if(in->mouse_dx||in->mouse_dy)G.kbd=0;if(!in->lmb)g_audio_drag=-1;
  if(G.click&&hover>=0&&hover<5){G.sel=hover;g_audio_drag=hover;}
  if(g_audio_drag>=0&&(in->lmb||G.click))audio_volume_set(g_audio_drag,((G.mx-70)*100+246)/492);
  int act=G.click?hover:(in->pressed[K_ENTER]||in->pressed[K_SPACE])?G.sel:-1;if(in->pressed[K_ESC])act=5;
  if(act==5){g_audio_drag=-1;setup_save();go(GS_OPTIONS);G.sel=3;return;}
  if(act>=0&&!G.click)audio_volume_set(act,g_audio_percent[act]>=100?0:g_audio_percent[act]+5);
  draw_cursor();
}
static void screen_retro_gfx(const Input *in){
  cv_pic(G.bg,0,0,1);cv_text(g_font_h14,320,6,"ERWEITERTE GRAFIK",ORANGE,1,1);
  char scaling[100]="";str_cpy(scaling,ui_translate("Skalierung (Retro Mode): "),sizeof scaling);str_int(scaling,sizeof scaling,G.gfx_scale);str_cat(scaling,"x",sizeof scaling);
  char filter[128]="";str_cpy(filter,ui_translate("Output filter: "),sizeof filter);str_cat(filter,RETRO_FILTER_NAMES[G.gfx_output_filter],sizeof filter);
  const char *labels[]={"Ganzzahlige Skalierung (Retro Mode)",scaling,"Vollbild",filter,"\xdc" "bernehmen"};int hover=-1;if(G.sel<0||G.sel>4)G.sel=0;
  for(int i=0;i<5;i++){
    int x=i==4?443:40,y=i==4?443:70+i*40;int on=i==0?G.gfx_intscale:i==2?G.gfx_full:0;
    const SprFile &sp=i==4?G.exit_spr:on?G.chk_on:G.chk_off;if(sp.n)cv_spr(spr_at(sp.f[0],x-3,y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h14,i==4?496:x+28,y,labels[i],G.kbd&&G.sel==i?WHITE:BEIGE,0,1);if(hit(x-3,y-4,i==4?200:590,30))hover=i;
  }
  cv_text(g_font_small,40,266,"Output filter hint",GREY,0,1);
  if(in->pressed[K_DOWN]){G.sel=(G.sel+1)%5;G.kbd=1;}if(in->pressed[K_UP]){G.sel=(G.sel+4)%5;G.kbd=1;}if(in->mouse_dx||in->mouse_dy)G.kbd=0;
  int dir=in->pressed[K_RIGHT]?1:in->pressed[K_LEFT]?-1:0;
  int act=G.click?hover:(in->pressed[K_ENTER]||in->pressed[K_SPACE])?G.sel:-1;if(dir&&(G.sel==1||G.sel==3))act=G.sel;if(in->pressed[K_ESC])act=4;
  if(act>=0){msfx(S_MENU2,0.7f);if(act==0)G.gfx_intscale^=1;if(act==1)G.gfx_scale=(G.gfx_scale-1+4+(dir?dir:1))%4+1;if(act==2)G.gfx_full^=1;if(act==3)G.gfx_output_filter=(G.gfx_output_filter+6+(dir?dir:1))%6;setup_save();if(act==4){go(GS_OPTIONS);G.sel=2;return;}}
  draw_cursor();
}
static void screen_retro_debug(const Input *in){
  cv_pic(G.bg,0,0,1);cv_text(g_font_h14,320,6,"DEBUGMEN\xdc",ORANGE,1,1);
  cv_text(g_font_h14,17,40,"Erweiterte Debug Optionen",ORANGE,0,1);
  cv_text(g_font_h14,17,260,"Cheats (nur Meisterschaft)",ORANGE,0,1);
  struct Row{const char *text;int *value;};Cheats &c=G.cheat_career;
  const Row rows[]={ {"FPS anzeigen",&G.dbg_fps},{"Race information",&G.dbg_info},{"Road markers",&G.dbg_road},{"Ohne Gegner fahren",&G.dbg_noopp},{"Skip videos",&G.dbg_noclips},
#ifdef _WIN32
    {"Debug-Konsole (CMD)",&G.dbg_console},
#else
    {"Debug-Ausgabe (Terminal)",&G.dbg_console},
#endif
    {"Unbegrenzt Geld",&c.money},{"Unbegrenzt Munition",&c.ammo},{"Alle Waffen und Wagen",&c.weapons},{"Kein Schaden",&c.nodamage},{"Zur\xfc" "cksetzen",0},{"\xdc" "bernehmen",0}};
  const int n=sizeof rows/sizeof rows[0];if(G.sel<0||G.sel>n)G.sel=0;int hover=-1;
  for(int i=0;i<n;i++){
    int x=i==11?443:40,y=i<6?70+i*31:i<10?300+(i-6)*28:443;
    const SprFile &sp=i==11?G.exit_spr:rows[i].value&&*rows[i].value?G.chk_on:G.chk_off;if(sp.n)cv_spr(spr_at(sp.f[0],x-3,y-2),G.bg.pal,0,0,1);
    cv_text(g_font_h14,i==11?496:x+28,y,rows[i].text,G.sel==i&&G.kbd?WHITE:BEIGE,0,1);
    if(hit(x-3,y-4,i==11?200:30+cv_text_w(g_font_h14,rows[i].text,1),28))hover=i;
  }
  cv_text(g_font_small,320,418,"Die Cheats f\xfcr das Einzelrennen stellst du im Einzelrennen-Men\xfc ein.",GREY,1,1);
  if(in->pressed[K_DOWN]||in->pressed[K_RIGHT]){G.sel=(G.sel+1)%(n+1);G.kbd=1;}if(in->pressed[K_UP]||in->pressed[K_LEFT]){G.sel=(G.sel+n)%(n+1);G.kbd=1;}if(in->mouse_dx||in->mouse_dy)G.kbd=0;
  int act=G.click?hover:(in->pressed[K_ENTER]||in->pressed[K_SPACE])?G.sel:-1;if(in->pressed[K_ESC])act=n-1;
  if(G.exit_spr.n)cv_spr(spr_at(G.exit_spr.f[0],440,222),G.bg.pal,0,0,1);cv_text(g_font_h14,493,224,"Extras",G.sel==n?WHITE:BEIGE,0,1);
  if((G.click&&hit(440,220,185,28))||(G.sel==n&&(in->pressed[K_ENTER]||in->pressed[K_SPACE]))){go(GS_EXTRAS);return;}
  if(act>=0&&act<n){msfx(S_MENU2,0.7f);if(rows[act].value)*rows[act].value^=1;
    if(act==n-2){G.dbg_console=G.dbg_fps=G.dbg_info=G.dbg_road=G.dbg_noopp=G.dbg_noclips=0;mem_zero(&c,sizeof c);}
    debug_apply();setup_save();if(act==n-1){go(GS_OPTIONS);G.sel=10;return;}}
  draw_cursor();
}
