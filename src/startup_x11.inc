int sys_choose_mode(int current,const char *title,const char *question,const char *revived,const char *retro,const char *remember,int *save){
  if(!g_display)return -1;Display *d=g_display;int screen=DefaultScreen(d),width=540,height=190;
  Window win=XCreateSimpleWindow(d,DefaultRootWindow(d),(DisplayWidth(d,screen)-width)/2,(DisplayHeight(d,screen)-height)/2,width,height,1,BlackPixel(d,screen),WhitePixel(d,screen));
  XStoreName(d,win,title);if(g_window)XSetTransientForHint(d,win,g_window);XSelectInput(d,win,ExposureMask|ButtonPressMask|KeyPressMask);
  Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,win,&close,1);
  XSizeHints hints={};hints.flags=PMinSize|PMaxSize;hints.min_width=hints.max_width=width;hints.min_height=hints.max_height=height;XSetWMNormalHints(d,win,&hints);
  GC gc=XCreateGC(d,win,0,0);XSetForeground(d,gc,BlackPixel(d,screen));XFontStruct *font=XLoadQueryFont(d,"fixed");if(font)XSetFont(d,gc,font->fid);
  XMapRaised(d,win);int result=-2,focus=current?1:0,checked=*save?1:0;
  auto paint=[&](){XClearWindow(d,win);XDrawString(d,win,gc,22,32,question,(int)strlen(question));
    const char *labels[]={revived,retro};for(int i=0;i<2;i++){int x=22+i*260;XDrawRectangle(d,win,gc,x,58,236,42);if(focus==i)XDrawRectangle(d,win,gc,x+3,61,230,36);XDrawString(d,win,gc,x+15,84,labels[i],(int)strlen(labels[i]));}
    XDrawRectangle(d,win,gc,22,131,18,18);if(checked){XDrawLine(d,win,gc,25,134,37,146);XDrawLine(d,win,gc,37,134,25,146);}if(focus==2)XDrawRectangle(d,win,gc,19,128,24,24);XDrawString(d,win,gc,50,145,remember,(int)strlen(remember));XFlush(d);};
  while(result==-2){XEvent e;XNextEvent(d,&e);if(e.xany.window!=win)continue;
    if(e.type==Expose)paint();
    if(e.type==ClientMessage&&(Atom)e.xclient.data.l[0]==close)result=-1;
    if(e.type==ButtonPress){int x=e.xbutton.x,y=e.xbutton.y;if(y>=58&&y<=100){if(x>=22&&x<=258)result=0;if(x>=282&&x<=518)result=1;}if(y>=128&&y<=156){checked^=1;focus=2;paint();}}
    if(e.type==KeyPress){KeySym k=XLookupKeysym(&e.xkey,0);if(k==XK_Escape)result=-1;else if(k==XK_Tab){focus=(focus+1)%3;paint();}else if(k==XK_Left||k==XK_Right){focus=focus==0?1:0;paint();}else if(k==XK_Return||k==XK_space){if(focus==2){checked^=1;paint();}else result=focus;}}
  }
  *save=checked;XFreeGC(d,gc);if(font)XFreeFont(d,font);XDestroyWindow(d,win);XFlush(d);return result;
}
int sys_game_is_gog(){return gf_status==GOG_COMPLETE&&gf_language>=0&&gf_language<3;}
int sys_run_dos(const char *directory,char *error,int cap){
  (void)directory;snprintf(error,cap,"Launching the DOS version is available in the Windows port only.");return 0;
}
