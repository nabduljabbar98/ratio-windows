'use client';
import { useEffect, useReducer, useRef, useState } from 'react';
import { BatteryFull, Wifi, Search, SlidersHorizontal, Command, PenTool, Globe, FileText, MessageCircle } from 'lucide-react';
import { flushSync } from 'react-dom';
import { Tabs, TabsList, TabsTrigger, TabsContent } from '@/components/ui/tabs';

type Mode = 'create' | 'consume' | null;
const applications = [
  {id:'figma', name:'Figma', activity:'Figma', icon:PenTool, description:'A place to make.', color:'violet'},
  {id:'chrome', name:'Chrome', activity:'x.com', icon:Globe, description:'A place to scroll.', color:'blue'},
  {id:'messages', name:'Messages', activity:'Messages', icon:MessageCircle, description:'A conversation in progress.', color:'green'},
  {id:'notes', name:'Notes', activity:'Notes', icon:FileText, description:'What are you working on?', color:'yellow'},
] as const;
type AppID = typeof applications[number]['id'];
const chromeTabs = [{id:'chrome',title:'Social Media',activity:'x.com'}, {id:'essay',title:'Essay',activity:'Essay'}, {id:'github',title:'GitHub',activity:'github.com'}] as const;
type ChromeTab = typeof chromeTabs[number]['id'];
type ActivityID = AppID | 'essay' | 'github';
const activities = [{id:'figma',activity:'Figma'},...chromeTabs,{id:'messages',activity:'Messages'},{id:'notes',activity:'Notes'}] as const;
type Usage = {seconds:number; pending:number; mode:Mode};
type State = {active:AppID; stack:AppID[]; chromeTab:ChromeTab; usage:Record<ActivityID,Usage>; session:number; paused:boolean; open:boolean; pendingOnly:boolean; running:boolean; away:boolean};
const initial:State = {active:'figma',chromeTab:'chrome',stack:['messages','notes','chrome','figma'],usage:{messages:{seconds:0,pending:0,mode:'consume'},figma:{seconds:180,pending:0,mode:'create'},chrome:{seconds:60,pending:0,mode:'consume'},notes:{seconds:12,pending:0,mode:'create'},essay:{seconds:0,pending:0,mode:'create'},github:{seconds:0,pending:0,mode:'create'}},session:0,paused:false,open:true,pendingOnly:false,running:true,away:false};
type Action = {type:'tick';away:boolean}|{type:'switch';id:AppID}|{type:'tab';id:ChromeTab}|{type:'classify';id:ActivityID;mode:Exclude<Mode,null>}|{type:'pause'|'panel'|'pending'|'forget'|'quit'|'reset'|'launch'};
function activityID(s:State):ActivityID{return s.active==='chrome'?s.chromeTab:s.active;}
function reducer(s:State,a:Action):State {
  const current=activityID(s);
  switch(a.type){
    case 'tick': {if(s.paused||!s.running||a.away)return {...s,away:a.away}; const u=s.usage[current];return {...s,away:false,session:s.session+1,usage:{...s.usage,[current]:{...u,seconds:u.seconds+1,pending:u.pending+(u.mode===null?1:0)}}};}
    case 'switch': return {...s,active:a.id,session:s.active===a.id?s.session:0,stack:[...s.stack.filter(id=>id!==a.id),a.id],away:false};
    case 'tab':return {...s,active:'chrome',chromeTab:a.id,session:s.chromeTab===a.id&&s.active==='chrome'?s.session:0,stack:[...s.stack.filter(id=>id!=='chrome'),'chrome'],away:false};
    case 'classify': {const u=s.usage[a.id];return {...s,usage:{...s.usage,[a.id]:{...u,pending:0,mode:a.mode}}};}
    case 'pause':return {...s,paused:!s.paused};
    case 'panel':return {...s,open:!s.open};
    case 'pending':return {...s,pendingOnly:!s.pendingOnly};
    case 'forget':return {...s,usage:{...s.usage,[current]:{...s.usage[current],pending:s.usage[current].seconds,mode:null}}};
    case 'quit':return {...s,running:false,open:false};
    case 'launch':return {...s,running:true,open:true};
    case 'reset':return {...initial,usage:Object.fromEntries(Object.entries(initial.usage).map(([id,u])=>[id,{...u,seconds:0,pending:0}])) as State['usage']};
  }
}
const time=(n:number)=>n>=3600?`${Math.floor(n/3600)}:${String(Math.floor(n/60)%60).padStart(2,'0')}:${String(n%60).padStart(2,'0')}`:`${Math.floor(n/60)}:${String(n%60).padStart(2,'0')}`;
export default function Home({initialWallpaper}:{initialWallpaper:string}){
  const [wallpaper,setWallpaper]=useState(initialWallpaper);
  const [s,dispatch]=useReducer(reducer,initial);
  const lastInput=useRef(0);
  const ratioAnchor=useRef<HTMLButtonElement>(null);
  const [panelLeft,setPanelLeft]=useState<number|null>(null);
  const [caretLeft,setCaretLeft]=useState(180);
  useEffect(()=>{
    const align=()=>{
      const anchor=ratioAnchor.current;if(!anchor)return;
      const rect=anchor.getBoundingClientRect();
      const width=Math.min(360,window.innerWidth-24);
      const left=Math.max(12,Math.min(rect.left+rect.width/2-width/2,window.innerWidth-width-12));
      setPanelLeft(left);
      setCaretLeft(Math.max(16,Math.min(width-16,rect.left+rect.width/2-left)));
    };
    align();window.addEventListener('resize',align);
    const observer=new ResizeObserver(align);
    const bar=ratioAnchor.current?.closest('header');if(bar)observer.observe(bar);
    const status=ratioAnchor.current?.parentElement;if(status)observer.observe(status);
    return()=>{window.removeEventListener('resize',align);observer.disconnect()};
  },[s.open,s.running]);
  const [clock,setClock]=useState('');
  useEffect(()=>{
    const update=()=>setClock(new Date().toLocaleString('en-US',{weekday:'short',month:'short',day:'numeric',hour:'numeric',minute:'2-digit'}));
    update();const id=setInterval(update,1000);return()=>clearInterval(id);
  },[]);
  const [positions,setPositions]=useState<Partial<Record<AppID,{x:number;y:number}>>>({});
  const drag=useRef<{id:AppID;pointer:number;x:number;y:number;startX:number;startY:number;left:number;top:number;width:number;height:number}|null>(null);
  function startDrag(event:React.PointerEvent<HTMLButtonElement>,id:AppID){
    if(event.button!==0||window.matchMedia('(max-width:760px)').matches)return;
    const rect=event.currentTarget.closest('.app-window')!.getBoundingClientRect();
    const offset=positions[id]??{x:0,y:0};
    drag.current={id,pointer:event.pointerId,x:event.clientX,y:event.clientY,startX:offset.x,startY:offset.y,left:rect.left,top:rect.top,width:rect.width,height:rect.height};
    event.currentTarget.setPointerCapture(event.pointerId);
    dispatch({type:'switch',id});
    event.preventDefault();
  }
  function moveDrag(event:React.PointerEvent<HTMLButtonElement>){
    const d=drag.current;if(!d||d.pointer!==event.pointerId)return;
    const dx=Math.max(16-d.left,Math.min(event.clientX-d.x,window.innerWidth-16-d.left-d.width));
    const dy=Math.max(38-d.top,Math.min(event.clientY-d.y,window.innerHeight-90-d.top));
    setPositions(p=>({...p,[d.id]:{x:d.startX+dx,y:d.startY+dy}}));
  }
  function endDrag(event:React.PointerEvent<HTMLButtonElement>){
    if(drag.current?.pointer!==event.pointerId)return;
    drag.current=null;
    if(event.currentTarget.hasPointerCapture(event.pointerId))event.currentTarget.releasePointerCapture(event.pointerId);
  }
  function resetDemo(){setPositions({});dispatch({type:'reset'});}

  useEffect(()=>{
    if(!s.open||!s.running)return;
    const dismiss=(event:PointerEvent)=>{
      const target=event.target;
      if(target instanceof Element&&!target.closest('#ratio-panel, .ratio-toggle'))dispatch({type:'panel'});
    };
    const escape=(event:KeyboardEvent)=>{if(event.key==='Escape')dispatch({type:'panel'})};
    document.addEventListener('pointerdown',dismiss,true);
    document.addEventListener('keydown',escape);
    return()=>{document.removeEventListener('pointerdown',dismiss,true);document.removeEventListener('keydown',escape)};
  },[s.open,s.running]);
  useEffect(()=>{lastInput.current=Date.now();const touch=()=>{lastInput.current=Date.now()};window.addEventListener('pointermove',touch);window.addEventListener('pointerdown',touch);window.addEventListener('keydown',touch);const timer=setInterval(()=>dispatch({type:'tick',away:document.hidden||Date.now()-lastInput.current>=60000}),1000);return()=>{clearInterval(timer);window.removeEventListener('pointermove',touch);window.removeEventListener('pointerdown',touch);window.removeEventListener('keydown',touch)}},[]);
  useEffect(()=>{
    type Tool={name:string;description:string;inputSchema:object;annotations:{readOnlyHint:boolean};execute:(input:unknown)=>unknown};
    const context=(document as Document & {modelContext?:{registerTool:(tool:Tool,options:{signal:AbortSignal})=>void|Promise<void>}}).modelContext;
    if(!context?.registerTool)return;const lifecycle=new AbortController();
    try{void Promise.resolve(context.registerTool({name:'switch_demo_app',description:'Bring a simulated Figma, Chrome, Notes, or Messages window forward. Ratio tracks this fake foreground app only.',inputSchema:{type:'object',properties:{app:{type:'string',enum:['figma','chrome','notes','messages']}},required:['app'],additionalProperties:false},annotations:{readOnlyHint:false},execute(input){const value=input as {app?:string};if(!value||!applications.some(a=>a.id===value.app))throw new Error('Choose figma, chrome, notes, or messages');const id=value.app as AppID;flushSync(()=>dispatch({type:'switch',id}));return {activeApp:id}}},{signal:lifecycle.signal})).catch(()=>{});}catch{/* Optional browser API. */}return()=>lifecycle.abort();
  },[]);
  const active=applications.find(a=>a.id===s.active)!;
  const current=activityID(s);
  const mode=s.usage[current].mode;
  const tracking=s.running&&!s.paused&&!s.away;
  const create=Object.values(s.usage).reduce((sum,u)=>sum+(u.mode==='create'?u.seconds:0),0);
  const consume=Object.values(s.usage).reduce((sum,u)=>sum+(u.mode==='consume'?u.seconds:0),0);
  const totalTracked=Object.values(s.usage).reduce((sum,u)=>sum+u.seconds,0);
  const hasClassifiedTime=create+consume>0;
  const percent=hasClassifiedTime?Math.round(create/(create+consume)*100):0;
  const ratioLabel=hasClassifiedTime?`${percent}/${100-percent}`:'—/—';
  const pending=activities.filter(a=>s.usage[a.id].pending>0).length;
  const rows=activities.filter(a=>(s.usage[a.id].seconds>0||a.id===current)&&(!s.pendingOnly||s.usage[a.id].pending>0)).sort((a,b)=>s.usage[b.id].seconds-s.usage[a.id].seconds);
  return <main className="desktop" aria-label="Ratio desktop demo">
    <img className="desktop-wallpaper" src={wallpaper} alt="" aria-hidden="true" draggable={false} fetchPriority="high" onError={()=>setWallpaper('/os/wallpaper.png')}/><header className="menubar"><div className="menus" aria-hidden="true"><Command size={15}/><strong>{active.name}</strong></div><div className="status-items">{s.running?<button ref={ratioAnchor} className={`ratio-toggle ${tracking?(mode==='create'?'green':mode==='consume'?'red':''):''}`} aria-expanded={s.open} aria-controls="ratio-panel" aria-label="Open or close Ratio" onClick={()=>dispatch({type:'panel'})}>{!tracking?'Ⅱ':mode==='create'?'↑':mode==='consume'?'↓':'?'} {ratioLabel}</button>:<button className="ratio-toggle" onClick={()=>dispatch({type:'launch'})}>OPEN RATIO</button>}<div className="system-icons" aria-hidden="true"><BatteryFull size={21}/><Wifi size={16}/><Search size={15}/><SlidersHorizontal size={15}/></div><time className="menu-clock">{clock}</time></div></header>
    <section className="windows" aria-label="Demo app windows">
      {applications.map(app=>{const Icon=app.icon;const isActive=app.id===s.active;return <div key={app.id} className={`app-window ${app.id} ${isActive?'foreground':''}`} style={{zIndex:s.stack.indexOf(app.id)+1,'--drag-x':`${positions[app.id]?.x??0}px`,'--drag-y':`${positions[app.id]?.y??0}px`} as React.CSSProperties} onPointerDown={()=>dispatch({type:'switch',id:app.id})} aria-label={`${app.name} window`}>
        <button className="window-title" onPointerDown={e=>startDrag(e,app.id)} onPointerMove={moveDrag} onPointerUp={endDrag} onPointerCancel={endDrag} onLostPointerCapture={()=>{drag.current=null}} onClick={()=>dispatch({type:'switch',id:app.id})} aria-label={`Switch to ${app.name}`}><span className="traffic" aria-hidden="true"><i/><i/><i/></span><span>{app.name}</span><span className="window-indicator">{isActive?'ACTIVE':' '}</span></button>
        {app.id==='chrome'?<Tabs className="chrome-browser" value={s.chromeTab} onValueChange={value=>dispatch({type:'tab',id:value as ChromeTab})}><TabsList className="browser-tabs">{chromeTabs.map(tab=><TabsTrigger key={tab.id} value={tab.id}>{tab.title}</TabsTrigger>)}</TabsList>
          <TabsContent value="chrome" className="browser-content"><Globe size={32} strokeWidth={1.2}/><span>x.com</span><p>A stream of other people’s ideas.</p></TabsContent>
          <TabsContent value="essay" className="browser-content essay-content"><label htmlFor="essay">UNTITLED ESSAY</label><textarea id="essay" aria-label="Write your essay" placeholder="Start with a thought…" onFocus={()=>dispatch({type:'tab',id:'essay'})}/></TabsContent>
          <TabsContent value="github" className="browser-content"><span className="wiki-icon" aria-hidden="true">⌘</span><span>GitHub</span><p>A place to build together.</p></TabsContent>
        </Tabs>:<button className="window-content" aria-label={`Switch to ${app.name}`} onClick={()=>dispatch({type:'switch',id:app.id})}><Icon className={`app-icon ${app.color}`} size={64} strokeWidth={1.2}/><span>{app.description}</span></button>}
      </div>})}
    </section>
    {s.open&&s.running&&<aside id="ratio-panel" className="ratio-panel" style={{...(panelLeft===null?{}:{left:panelLeft,right:'auto'}),'--caret-left':`${caretLeft}px`} as React.CSSProperties} aria-label="Ratio activity tracker">
      <div className="ratio-bar" role="img" aria-label={hasClassifiedTime?`${percent}% create, ${100-percent}% consume`:'No categorized time'}><span style={{width:`${percent}%`}}/></div>
      <div className="summary"><span><span className="green">↑</span>&nbsp;{time(create)} CREATING</span><span><span className="red">↓</span>&nbsp;{time(consume)} CONSUMING</span></div>
      <div className="tracking-row"><span className="tracking-label">{s.pendingOnly?'TO CATEGORIZE':s.paused?'PAUSED':s.away?'AWAY':'TRACKING'}</span><span className="tracking-total" aria-label="Total tracked time">{time(totalTracked)}</span><button className={s.pendingOnly?'selected':''} aria-pressed={s.pendingOnly} aria-label={s.pendingOnly?'Show all activity':`Review ${pending} uncategorized apps`} onClick={()=>dispatch({type:'pending'})}>{pending?`! ${pending}`:'✓'}</button></div>
      <div className="activity-list" aria-label={s.pendingOnly?'Uncategorized activity':'Today’s activity'}>{rows.length?rows.map(app=><div className="activity-row" key={app.id}><span className="activity-name">{app.activity}</span><span className="activity-time">{tracking&&current===app.id&&<span className="live-dot" aria-label="Currently tracking"/>}{time(s.usage[app.id].seconds)}</span><button className={`arrow green ${s.usage[app.id].mode==='create'?'selected':''}`} aria-pressed={s.usage[app.id].mode==='create'} aria-label={`Categorize ${app.activity} as create`} onClick={()=>dispatch({type:'classify',id:app.id,mode:'create'})}>↑</button><button className={`arrow red ${s.usage[app.id].mode==='consume'?'selected':''}`} aria-pressed={s.usage[app.id].mode==='consume'} aria-label={`Categorize ${app.activity} as consume`} onClick={()=>dispatch({type:'classify',id:app.id,mode:'consume'})}>↓</button></div>):<p className="empty">All caught up.</p>}</div>
      <div className="panel-footer"><button onClick={()=>dispatch({type:'pause'})}>{s.paused?'RESUME':'PAUSE'}</button><button aria-label="Reset the entire demo" onClick={resetDemo}>RESET</button><button onClick={()=>dispatch({type:'quit'})}>QUIT</button></div>
    </aside>}
    <div className="desktop-caption"><button className="reset" onClick={resetDemo}>RESET DEMO ↺</button></div>
  </main>
}
