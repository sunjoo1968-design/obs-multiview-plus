const fs=require('fs'),path=require('path'),net=require('net'),crypto=require('crypto'),assert=require('assert');
const {spawn,spawnSync}=require('child_process');
const root=path.resolve(__dirname,'..'),sandbox=path.join(root,'.local/hybrid-runtime/obs');
const lua=process.env.MV_HYBRID_LUA,python=process.env.MV_TEST_PYTHON;
assert(lua&&python,'Set MV_HYBRID_LUA and MV_TEST_PYTHON');
assert(typeof WebSocket==='function','Node.js 22+ required');
const exe=path.join(sandbox,'bin/64bit/obs64.exe'),cfg=path.join(sandbox,'config/obs-studio');
const generated=path.join(root,'.local/hybrid-runtime/controller.lua'),ready=path.join(path.dirname(generated),'hybrid-ready.txt'),state=path.join(path.dirname(generated),'tile-state.json');
fs.writeFileSync(generated,fs.readFileSync(lua,'utf8')+'\n'+fs.readFileSync(path.join(root,'tests/hybrid-fixture.lua'),'utf8'));
function put(name,data){const p=path.join(cfg,name);fs.mkdirSync(path.dirname(p),{recursive:true});fs.writeFileSync(p,data);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
class Client{
 async connect(url,password){this.pending=new Map();this.sequence=0;this.socket=new WebSocket(url);await new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>reject(Error('Websocket hello timeout')),3000);
  this.socket.addEventListener('error',()=>{clearTimeout(timer);reject(Error('Connection failed'));});
  this.socket.addEventListener('message',event=>{const msg=JSON.parse(event.data);
   if(msg.op===0){const d={rpcVersion:1};if(msg.d.authentication){const hash=value=>crypto.createHash('sha256').update(value).digest('base64');d.authentication=hash(hash(password+msg.d.authentication.salt)+msg.d.authentication.challenge);}this.socket.send(JSON.stringify({op:1,d}));}
   if(msg.op===2){clearTimeout(timer);resolve();}
   if(msg.op===7){const p=this.pending.get(msg.d.requestId);if(p){this.pending.delete(msg.d.requestId);msg.d.requestStatus.result?p.resolve(msg.d.responseData):p.reject(Error(JSON.stringify(msg.d.requestStatus)));}}
  });
 });}
 call(type,data={}){const id=String(++this.sequence);return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{this.pending.delete(id);reject(Error('Request timeout '+type));},5000);this.pending.set(id,{resolve:x=>{clearTimeout(timer);resolve(x);},reject:e=>{clearTimeout(timer);reject(e);}});this.socket.send(JSON.stringify({op:6,d:{requestType:type,requestId:id,requestData:data}}));});}
 close(){this.socket?.close();}
}
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));return p;}
async function checkTiles(pgm){const preview=3-pgm;let rows=[];for(let i=0;i<40;i++){try{rows=JSON.parse(fs.readFileSync(state,'utf8'));}catch{}const good=[['Test Scene '+pgm,true,false],['Test Camera '+pgm,true,false],['Test Scene '+preview,false,true],['Test Camera '+preview,false,true]].every(([name,red,green])=>rows.some(row=>row.name===name&&row.red===red&&row.green===green));if(good)return rows;await pause(100);}throw Error('Wrong actual tile colors '+JSON.stringify(rows));}
(async()=>{
const port=await freePort(),password=crypto.randomBytes(20).toString('hex');const ini='[General]\nFirstRun=true\nLanguage=en-US\n[Basic]\nProfile=HybridMV\nProfileDir=HybridMV\nSceneCollection=HybridMV\nSceneCollectionFile=HybridMV\n[BasicWindow]\nWarnBeforeExit=false\nPreviewEnabled=true\nSceneDuplicationMode=true\nEditPropertiesMode=true\n';
put('global.ini',ini);put('user.ini',ini);
put('basic/profiles/HybridMV/basic.ini','[General]\nName=HybridMV\n[Video]\nBaseCX=1280\nBaseCY=720\nOutputCX=1280\nOutputCY=720\nFPSCommon=30\n[Output]\nMode=Simple\n');
put('plugin_config/obs-websocket/config.json',JSON.stringify({first_load:false,server_enabled:true,server_port:port,alerts_enabled:false,auth_required:true,server_password:password}));
const sources=[];for(const i of [1,2]){sources.push({name:`Test Camera ${i}`,id:'color_source_v3',versioned_id:'color_source_v3',settings:{width:1280,height:720,color:i===1?0xff0000ff:0xff00ff00}});sources.push({name:`Test Scene ${i}`,id:'scene',settings:{items:[{name:`Test Camera ${i}`,id:i*101,visible:true,pos:{x:0,y:0},scale:{x:1,y:1},rot:0,align:5}]}});}
put('basic/scenes/HybridMV.json',JSON.stringify({name:'HybridMV',current_scene:'Test Scene 1',current_program_scene:'Test Scene 1',scene_order:[{name:'Test Scene 1'},{name:'Test Scene 2'}],sources,transition_duration:300,modules:{'scripts-tool':[{path:generated.replaceAll('\\','/'),settings:{}}]}}));
for(const p of [ready,state])if(fs.existsSync(p))fs.unlinkSync(p);
const child=spawn(exe,['--portable','--multi','--disable-shutdown-check','--disable-updater','--disable-missing-files-check','--profile','HybridMV','--collection','HybridMV'],{cwd:path.dirname(exe),windowsHide:true,stdio:'ignore',env:{...process.env,MV_HYBRID_STATE:state}});
const exited=new Promise(resolve=>child.once('exit',code=>resolve(code)));let client;
try{
for(let i=0;i<100;i++){assert(child.exitCode===null);client=new Client();try{await client.connect(`ws://127.0.0.1:${port}`,password);break;}catch{client.close();client=null;await pause(150);}}
assert(client);for(let i=0;i<100&&!fs.existsSync(ready);i++)await pause(100);assert(fs.existsSync(ready),'Hybrid setup did not finish');
await client.call('SetStudioModeEnabled',{studioModeEnabled:true});
for(const pgm of [1,2]){
 await client.call('TriggerHotkeyByName',{hotkeyName:`camera_mix_hybrid.camera_${pgm}`});await pause(250);
 await client.call('SetCurrentPreviewScene',{sceneName:'Hybrid ME1 PGM Output'});await client.call('TriggerStudioModeTransition');await pause(500);
 await client.call('SetCurrentPreviewScene',{sceneName:'Hybrid ME1 PGM Output'});await client.call('TriggerHotkeyByName',{hotkeyName:`camera_mix_hybrid.camera_${3-pgm}`});await pause(250);
 if(process.env.MV_TEST_TALLY_VERSION){
  const response=await client.call('CallVendorRequest',{vendorName:'sunjooan-tally',requestType:'GetSnapshot'});
  const data=response.responseData;assert(data.ready&&data.version===process.env.MV_TEST_TALLY_VERSION);
  assert(data.suite==='Sunjoo OBS Link');
  assert(data['camera-mix-hybrid'].includes('Sunjoo OBS Link Controller'));
  assert(data['obs-multiview-plus'].includes('Sunjoo OBS Link Multiview'));
  for(const [bus,camera] of [['program',pgm],['preview',3-pgm]]){
   const name=`Test Scene ${camera}`;assert(data[`${bus}Scenes`].some(x=>x.name===name));
   const view=data[`${bus}Views`].find(x=>x.name===name);
   assert(view&&view.visible.some(x=>x.name===`Test Camera ${camera}`));
   assert(!data[`${bus}Scenes`].some(x=>x.name===`Test Scene ${3-camera}`));
  }
  console.log(`PASS Sunjoo OBS Link integration: Tally and Multiview agree on PGM ${pgm}/PVW ${3-pgm}`);
 }
 const rows=await checkTiles(pgm);fs.writeFileSync(path.join(path.dirname(state),`tile-state-pgm${pgm}.json`),JSON.stringify(rows,null,2));
 console.log(`PASS actual Multiview Hybrid: ME1 PGM scene/source ${pgm} RED, Preview ${3-pgm} GREEN`);
}
const result=spawnSync(python,[path.join(root,'tests/close-hybrid-obs.py'),String(child.pid),exe],{windowsHide:true,encoding:'utf8'});assert(result.status===0,result.stderr);
assert.strictEqual(await Promise.race([exited,pause(15000).then(()=>{throw Error('Normal shutdown timeout');})]),0);
console.log('PASS actual Multiview Hybrid normal shutdown exit=0');
}finally{client?.close();if(child.exitCode===null){spawnSync(python,[path.join(root,'tests/close-hybrid-obs.py'),String(child.pid),exe],{windowsHide:true});await Promise.race([exited,pause(5000)]);if(child.exitCode===null)child.kill();}}
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
