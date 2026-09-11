
// Frontend controls are connected to the ESP32 WebServerManager API.
const copy={dashboard:['Dashboard',"A quick view of your frame's health and display."],wifi:['WiFi','Connect your frame to a nearby network.'],gallery:['Gallery','Manage the photos shown on your frame.'],weather:['Weather','Choose the location shown on your frame.'],settings:['Settings','Personalize the frame and manage device actions.']};const root=document.documentElement,title=document.getElementById('title'),desc=document.getElementById('desc');
document.querySelectorAll('.nav button').forEach(b=>b.onclick=()=>{const tab=b.dataset.tab;document.querySelectorAll('.nav button').forEach(x=>x.classList.toggle('active',x===b));document.querySelectorAll('.panel').forEach(x=>x.classList.toggle('active',x.id===tab));title.textContent=copy[tab][0];desc.textContent=copy[tab][1];scrollTo({top:0,behavior:'smooth'});});
const theme=document.getElementById('theme'),toggle=document.getElementById('darkSwitch');function setTheme(d){root.dataset.theme=d?'dark':'light';toggle.classList.toggle('on',d);toggle.setAttribute('aria-checked',d);localStorage.setItem('photoFrameTheme',d?'dark':'light')}setTheme(localStorage.getItem('photoFrameTheme')==='dark');theme.onclick=()=>setTheme(root.dataset.theme!=='dark');toggle.onclick=()=>setTheme(root.dataset.theme!=='dark');
// AI CHANGE: persist the selected accent palette independently from light/dark mode.
const accentTheme=document.getElementById('accentTheme');function setAccentTheme(name){const allowed=['orange','neon','green','yellow','red','blue'];const value=allowed.includes(name)?name:'orange';root.dataset.accent=value;if(accentTheme)accentTheme.value=value;localStorage.setItem('photoFrameAccent',value)}setAccentTheme(localStorage.getItem('photoFrameAccent')||'orange');accentTheme?.addEventListener('change',e=>setAccentTheme(e.target.value));
let brightnessTimer;
document.querySelectorAll('.range').forEach(r=>r.oninput=e=>{document.querySelectorAll('.range').forEach(x=>x.value=e.target.value);document.getElementById('brightVal').textContent=e.target.value+'%';document.getElementById('settingsBright').textContent=e.target.value;clearTimeout(brightnessTimer);brightnessTimer=setTimeout(()=>{const v=Math.round(parseInt(e.target.value,10)*255/100);fetch('/api/system/brightness',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'value='+v});},150);});
// AI CHANGE: clicking an SSID reveals the password form and remembers the selected SSID.
function selectNetwork(ssid){const value=String(ssid||'').trim();if(!value)return;document.getElementById('ssid').value=value;const credentials=document.getElementById('wifiCredentials');credentials.hidden=false;document.querySelectorAll('.network').forEach(row=>row.classList.toggle('selected',row.dataset.ssid===value));document.getElementById('pass').focus()}
// [PATCH wifi-tab] Ngưỡng RSSI coi là "sóng mạnh" khi lọc kết quả Scan WiFi.
// Thang tham khảo: Excellent > -60dBm, Good -60~-70dBm, Fair -70~-80dBm, Weak < -80dBm.
// Đang lấy mốc Good trở lên (>= -75dBm) — chỉnh số này nếu muốn lọc chặt/lỏng hơn.
const STRONG_SIGNAL_RSSI_THRESHOLD=-75;

// [PATCH wifi-tab] source hiện tại của danh sách: 'connected' (mặc định, hiện mạng đang dùng)
// hoặc 'scan' (sau khi bấm nút Scan WiFi) — tránh việc poll /api/system mỗi 5s đè mất kết quả scan.
let _wifiListSource='connected';
let _currentConnectedSsid=null;

function signalBarsHtml(rssi){
  // 4 vạch sóng theo cường độ, dùng chung 1 style với bản demo cũ
  const bars=rssi>=-55?4:rssi>=-67?3:rssi>=-78?2:1;
  let html='';
  for(let i=1;i<=4;i++)html+='<i style="height:'+(4+i*3)+'px;opacity:'+(i<=bars?(i===4?1:.45+i*.15):.18)+'"></i>';
  return html;
}

function renderNetworks(networks){
  const list=document.getElementById('networkList');
  list.replaceChildren();
  if(!networks.length){
    const empty=document.createElement('div');
    empty.className='networksEmpty';
    empty.textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Không tìm thấy mạng WiFi nào đủ mạnh.':'No strong WiFi networks found.';
    list.appendChild(empty);
    return;
  }
  networks.forEach(network=>{
    const row=document.createElement('div');
    row.className='network'+(network.ssid&&network.ssid===_currentConnectedSsid?' connected':'');
    row.dataset.ssid=network.ssid||'';
    const icon=document.createElement('span');
    icon.className='wifi';
    icon.innerHTML=navIconSvg.wifi;
    const svg=icon.firstElementChild;
    svg.setAttribute('width','20');
    svg.setAttribute('height','20');
    const name=document.createElement('span');
    name.className='networkName';
    name.textContent=network.ssid||'(hidden network)';
    if(network.ssid&&network.ssid===_currentConnectedSsid){
      const tag=document.createElement('span');
      tag.className='networkConnectedTag';
      tag.textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'ĐANG DÙNG':'CONNECTED';
      name.appendChild(tag);
    }
    const meta=document.createElement('span');
    meta.className='networkMeta';
    meta.textContent=(network.rssi??'—')+' dBm';
    const signal=document.createElement('span');
    signal.className='signal';
    signal.innerHTML=signalBarsHtml(network.rssi??-90);
    row.append(icon,name,meta,signal);
    row.onclick=()=>selectNetwork(row.dataset.ssid);
    list.appendChild(row);
  });
}

// [PATCH wifi-tab] Hiện đúng mạng đang kết nối thật (đọc từ /api/system) thay vì dữ liệu demo/scan cũ,
// gọi mỗi lần fetchSystemStatus() poll, CHỈ khi user chưa bấm Scan trong phiên này (_wifiListSource==='connected').
function renderConnectedNetworkOnly(status){
  _currentConnectedSsid=status.wifi&&status.wifi.connected?status.wifi.ssid:null;
  if(_wifiListSource!=='connected')return; // đã scan rồi thì không ghi đè danh sách scan
  if(_currentConnectedSsid){
    renderNetworks([{ssid:_currentConnectedSsid,rssi:status.wifi.rssi}]);
  }else{
    renderNetworks([]); // chưa kết nối mạng nào -> hiện trạng thái rỗng, không hiện demo nữa
  }
}
document.querySelectorAll('.network').forEach(row=>row.onclick=()=>selectNetwork(row.dataset.ssid));
async function scanWifi(){const state=document.getElementById('scanState');state.dataset.state='scanning';state.textContent=languageData[activeLanguage].scanScanning;try{const response=await fetch('/api/wifi/scan');if(!response.ok)throw new Error('scan failed');const networks=await response.json();
  // [PATCH wifi-tab] Chỉ giữ lại mạng có sóng đủ mạnh (xem STRONG_SIGNAL_RSSI_THRESHOLD phía trên),
  // vẫn sắp theo RSSI giảm dần để mạng mạnh nhất hiện đầu danh sách.
  const strongNetworks=networks.filter(n=>(n.rssi??-999)>=STRONG_SIGNAL_RSSI_THRESHOLD).sort((a,b)=>(b.rssi??-999)-(a.rssi??-999));
  _wifiListSource='scan'; // từ giờ không cho poll /api/system ghi đè danh sách này nữa
  renderNetworks(strongNetworks);
  state.dataset.state='found';state.textContent=languageData[activeLanguage].scanFound.replace(/^\d+/,String(strongNetworks.length||0))}catch(error){state.dataset.state='error';state.textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Không thể quét WiFi.':'Unable to scan WiFi.'}}async function connectWifi(){const ssid=document.getElementById('ssid').value.trim(),password=document.getElementById('pass').value;if(!ssid)return;const body=new URLSearchParams({ssid,password});try{const response=await fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!response.ok)throw new Error('connect failed');document.getElementById('connected').textContent=ssid;document.getElementById('scanState').textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Đã lưu. Đang kết nối lại…':'Saved. Reconnecting…'}catch(error){document.getElementById('scanState').textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Không thể lưu cài đặt WiFi.':'Unable to save WiFi settings.'}}async function resetWifi(){try{const response=await fetch('/api/wifi/reset',{method:'POST'});if(!response.ok)throw new Error('reset failed');document.getElementById('ssid').value='';document.getElementById('pass').value='';document.getElementById('wifiCredentials').hidden=true;document.querySelectorAll('.network').forEach(row=>row.classList.remove('selected'));_wifiListSource='connected';/* [PATCH wifi-tab] quay về hiện trạng thái thật sau khi reset, không giữ list scan cũ */document.getElementById('scanState').textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Đã đặt lại WiFi. Đang kết nối lại AP cài đặt…':'WiFi reset. Reconnecting to setup AP…'}catch(error){document.getElementById('scanState').textContent=(typeof languageData!=='undefined'&&activeLanguage==='vi')?'Không thể đặt lại WiFi.':'Unable to reset WiFi.'}}document.getElementById('connect').onclick=connectWifi;document.getElementById('resetWiFi').onclick=resetWifi;
function fmtBytes(b){if(b<1024)return b+' B';if(b<1024*1024)return(b/1024).toFixed(1)+' KB';if(b<1024*1024*1024)return(b/1024/1024).toFixed(1)+' MB';return(b/1024/1024/1024).toFixed(2)+' GB'}
async function fetchSystemStatus(){
  try{
    const r=await fetch('/api/system');
    if(!r.ok)throw new Error('status fetch failed');
    const s=await r.json();

    // Dashboard - 4 stat card
    document.getElementById('statCpu').textContent=s.cpuTemperatureSupported?s.cpuTemperatureC.toFixed(1)+'°C':'N/A';
    document.getElementById('statCpuNote').textContent=s.cpuTemperatureSupported?(s.cpuTemperatureC<60?'Normal range':'Running hot'):'Not supported';
    document.getElementById('statRam').textContent=fmtBytes(s.freeHeap);
    document.getElementById('statRamNote').textContent='of '+fmtBytes(s.totalHeap)+' total';
    document.getElementById('statWifi').textContent=s.wifi.connected?s.wifi.rssi+' dBm':'Offline';
    document.getElementById('statWifiNote').textContent=s.wifi.connected?(s.wifi.rssi>-67?'Good connection':'Weak connection'):(s.mode==='ap'?'Setup AP active':'Disconnected');
    document.getElementById('statSd').textContent=s.sd.mounted?'Ready':(s.sd.cardPresent?'Not mounted':'No card');
    document.getElementById('statSdNote').textContent=s.sd.mounted?fmtBytes(s.sd.freeBytes)+' free':'—';

    // Dashboard - badge trạng thái Live/AP
    document.getElementById('liveBadge').textContent=s.mode==='ap'?'● Setup mode':'● Live';

    // Dashboard - System information card
    document.getElementById('infoUptime').textContent=s.uptime;
    document.getElementById('infoIp').textContent=s.ip||'—';
    document.getElementById('infoRssi').textContent=s.wifi.connected?s.wifi.rssi+' dBm':'N/A';
    document.getElementById('infoSd').textContent=s.sd.mounted?'Mounted · '+fmtBytes(s.sd.totalBytes):(s.sd.cardPresent?'Present, not mounted':'Not mounted');

    // Tab WiFi
    document.getElementById('connected').textContent=s.mode==='ap'?'(Setup AP mode)':(s.wifi.ssid||'—');
    document.getElementById('connIp').textContent=s.ip||'—';
    renderConnectedNetworkOnly(s); // [PATCH wifi-tab] cập nhật list mạng theo trạng thái thật, trừ khi đang hiện kết quả Scan

    // Tab Gallery - SD storage overview
    document.getElementById('storageCapacity').textContent=s.sd.mounted?fmtBytes(s.sd.totalBytes):'—';
    document.getElementById('storageUsed').textContent=s.sd.mounted?fmtBytes(s.sd.usedBytes):'—';
    document.getElementById('storageFree').textContent=s.sd.mounted?fmtBytes(s.sd.freeBytes):'—';
    if(s.sd.mounted&&s.sd.totalBytes>0){
      const pct=Math.min(100,(s.sd.usedBytes/s.sd.totalBytes*100));
      document.getElementById('storageFill').style.width=pct.toFixed(1)+'%';
    }
    // storageCount (tổng số ảnh) chưa có endpoint backend trả về -> giữ nguyên, cần route /api/gallery riêng nếu muốn đồng bộ

    applyFeatureFlags(s.features);
  }catch(error){
    console.error('fetchSystemStatus failed',error);
  }
}
fetchSystemStatus();
setInterval(fetchSystemStatus,5000);

function renderGalleryList(images){
  const list=document.getElementById('galleryList');
  list.replaceChildren();
  list.classList.toggle('scrollMode', images.length>=4); // ≥4 ảnh -> khung scroll ngang, dưới 4 -> lưới bình thường

  if(!images.length){
    const empty=document.createElement('p');
    empty.className='help';
    empty.textContent='No JPG files found on the SD card.';
    list.appendChild(empty);
    return;
  }
  images.forEach(img=>{
    const item=document.createElement('div');
    item.className='galleryItem';

    const thumb=document.createElement('img');
    thumb.className='thumb';
    thumb.loading='lazy';
    thumb.alt=img.name;
    thumb.src='/gallery/file?path='+encodeURIComponent(img.path);

    const file=document.createElement('div');
    file.className='file';
    const name=document.createElement('div');
    name.className='fileName';
    name.textContent=img.name;
    const size=document.createElement('div');
    size.className='fileSize';
    size.textContent=fmtBytes(img.sizeBytes);
    file.append(name,size);

    const del=document.createElement('button');
    del.className='delete';
    del.textContent='×';
    del.dataset.feature='galleryDelete'; // applyFeatureFlags tự khoá/mở nút này theo backend
    del.onclick=()=>deleteImage(img.path);

    item.append(thumb,file,del);
    list.appendChild(item);
  });
}

async function fetchGallery(){
  try{
    const r=await fetch('/api/gallery');
    if(!r.ok)throw new Error('gallery fetch failed');
    const g=await r.json();
    const images=g.images||[];
    renderGalleryList(images);
    document.getElementById('count').textContent=images.length+' image'+(images.length===1?'':'s')+' · sorted by name';
    document.getElementById('storageCount').textContent=images.length;

    const intervalField=document.getElementById('interval');
    if(document.activeElement!==intervalField&&typeof g.slideIntervalSec==='number'){
      intervalField.value=g.slideIntervalSec; // không ghi đè khi user đang gõ dở
    }

    applyFeatureFlags(_lastFeatures); // các nút delete mới render cần được áp flag ngay
  }catch(error){
    console.error('fetchGallery failed',error);
    document.getElementById('count').textContent='Unable to load gallery.';
  }
}

document.getElementById('interval').addEventListener('change',async e=>{
  const seconds=Math.max(1,Math.min(3600,parseInt(e.target.value,10)||5));
  e.target.value=seconds;
  try{
    const body=new URLSearchParams({seconds});
    const r=await fetch('/api/gallery/interval',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if(!r.ok)throw new Error('save interval failed');
  }catch(error){
    console.error('save interval failed',error);
    alert('Không thể lưu thời gian chuyển ảnh.');
  }
});

let _lastFeatures=null;
function applyFeatureFlags(features){
  if(!features)return;
  _lastFeatures=features;
  document.querySelectorAll('[data-feature]').forEach(el=>{
    const key=el.dataset.feature;
    const enabled=!!features[key];
    el.disabled=!enabled;
    el.style.opacity=enabled?'':'.45';
    el.style.pointerEvents=enabled?'':'none';
    if(enabled)el.removeAttribute('title');
    else el.title='Tính năng chưa khả dụng trên firmware hiện tại';
  });
}

fetchGallery();
setInterval(fetchGallery,10000); // gallery ít thay đổi hơn, poll thưa hơn system status
const file=document.getElementById('file'),choose=document.getElementById('choose'),drop=document.getElementById('drop'),progress=document.getElementById('progress');
const editToggle=document.getElementById('editMode'),uploadError=document.getElementById('uploadError');
let editMode=localStorage.getItem('photoFrameEditMode')==='on';
function setEditMode(enabled){editMode=!!enabled;editToggle.classList.toggle('on',editMode);editToggle.setAttribute('aria-checked',String(editMode));localStorage.setItem('photoFrameEditMode',editMode?'on':'off')}
setEditMode(editMode);editToggle.onclick=()=>setEditMode(!editMode);
function uploadMessage(key,fallback){const vi=typeof languageData!=='undefined'&&activeLanguage==='vi';const text={badType:vi?'Bỏ qua tệp không phải JPG/JPEG: ':'JPG/JPEG only: ',badSize:vi?'Ảnh phải có độ phân giải chính xác 320 × 240: ':'Image must be exactly 320 × 240: ',failed:vi?'Tải lên thất bại ':'Upload failed ',editorLoad:vi?'Không thể mở ảnh: ':'Unable to open image: '};return text[key]||fallback}
function showUploadError(message){uploadError.textContent=message;uploadError.classList.toggle('show',!!message)}
window.addEventListener('error',e=>{
  showUploadError('[Debug] JS error: '+(e.message||'unknown')+' @ line '+(e.lineno||'?'));
  console.error('window.onerror',e);
});
window.addEventListener('unhandledrejection',e=>{
  const reason=e.reason&&e.reason.message?e.reason.message:String(e.reason);
  showUploadError('[Debug] Promise error: '+reason);
  console.error('unhandledrejection',e.reason);
});
function isJpeg(f){return f.type?/^image\/jpe?g$/i.test(f.type):/\.jpe?g$/i.test(f.name)}
function readImage(f){return new Promise((resolve,reject)=>{const url=URL.createObjectURL(f),img=new Image();img.onload=()=>{URL.revokeObjectURL(url);resolve(img)};img.onerror=()=>{URL.revokeObjectURL(url);reject(new Error('image decode failed'))};img.src=url})}
// [NOTE upgrade-resolution] Hằng số độ phân giải khung ảnh hiện tại (khớp với TFT vật lý 320x240px).
// Nâng cấp màn hình lớn hơn sau này -> chỉ cần đổi 2 số này + width/height của <canvas id="editorCanvas">
// trong HTML (có ghi chú tại đó), không cần sửa logic crop/zoom/pinch/upload nào khác.
const FRAME_OUTPUT_WIDTH=320;
const FRAME_OUTPUT_HEIGHT=240;

async function validateDirect(f){if(!isJpeg(f))return uploadMessage('badType','JPG/JPEG only: ')+f.name;try{const img=await readImage(f);if(img.naturalWidth!==FRAME_OUTPUT_WIDTH||img.naturalHeight!==FRAME_OUTPUT_HEIGHT)return uploadMessage('badSize','Image must be exactly 320 × 240: ')+f.name+' ('+img.naturalWidth+' × '+img.naturalHeight+')';return ''}catch{return uploadMessage('editorLoad','Unable to read image: ')+f.name}}

function uploadSingleFile(f,index,total){return new Promise(resolve=>{const xhr=new XMLHttpRequest();xhr.open('POST','/api/gallery/upload');xhr.upload.onprogress=e=>{if(!e.lengthComputable)return;const pct=Math.round(e.loaded/e.total*100);document.getElementById('fill').style.width=pct+'%';document.getElementById('pct').textContent=total>1?`${index}/${total} · ${pct}%`:pct+'%'};xhr.onload=()=>resolve(xhr.status>=200&&xhr.status<300);xhr.onerror=()=>resolve(false);const form=new FormData();form.append('file',f,f.name);xhr.send(form)})}

const editorBackdrop=document.getElementById('editorBackdrop'),editorCanvas=document.getElementById('editorCanvas'),editorCtx=editorCanvas.getContext('2d'),editorZoom=document.getElementById('editorZoom');
const editorState={image:null,file:null,scale:1,rotation:0,x:0,y:0,dragging:false,lastX:0,lastY:0,resolve:null,pointers:new Map(),pinchStartDist:0,pinchStartScale:1};
function drawEditor(){const c=editorCanvas,ctx=editorCtx,img=editorState.image;if(!img)return;ctx.clearRect(0,0,c.width,c.height);ctx.fillStyle='#111';ctx.fillRect(0,0,c.width,c.height);const base=Math.max(c.width/img.naturalWidth,c.height/img.naturalHeight);ctx.save();ctx.translate(c.width/2+editorState.x,c.height/2+editorState.y);ctx.rotate(editorState.rotation*Math.PI/180);ctx.scale(base*editorState.scale,base*editorState.scale);ctx.drawImage(img,-img.naturalWidth/2,-img.naturalHeight/2);ctx.restore()}
function closeEditor(result){editorBackdrop.classList.remove('show');document.body.style.overflow='';const done=editorState.resolve;editorState.resolve=null;editorState.image=null;if(done)done(result)}
function openEditor(f,index,total){return readImage(f).then(img=>new Promise(resolve=>{editorState.image=img;editorState.file=f;editorState.scale=1;editorState.rotation=0;editorState.x=0;editorState.y=0;editorState.resolve=resolve;editorState.pointers.clear();editorState.pinchStartDist=0;editorState.dragging=false;document.getElementById('editorMeta').textContent=f.name+(total>1?' · '+(index+1)+'/'+total:'');editorZoom.value='1';editorBackdrop.classList.add('show');document.body.style.overflow='hidden';drawEditor()})).catch(()=>{showUploadError(uploadMessage('editorLoad','Unable to open image: ')+f.name);return null})}
function processedEditorFile(){return new Promise(resolve=>{editorCanvas.toBlob(blob=>{if(!blob)return resolve(null);const name=editorState.file.name.replace(/\.[^.]+$/,'')+'.jpg';resolve(new File([blob],name,{type:'image/jpeg'}))},'image/jpeg',.92)})}
editorZoom.oninput=()=>{editorState.scale=Number(editorZoom.value);drawEditor()};document.getElementById('zoomIn').onclick=()=>{editorZoom.value=Math.min(3,Number(editorZoom.value)+.1).toFixed(2);editorZoom.oninput()};document.getElementById('zoomOut').onclick=()=>{editorZoom.value=Math.max(1,Number(editorZoom.value)-.1).toFixed(2);editorZoom.oninput()};document.getElementById('rotateLeft').onclick=()=>{editorState.rotation=(editorState.rotation+270)%360;drawEditor()};document.getElementById('rotateRight').onclick=()=>{editorState.rotation=(editorState.rotation+90)%360;drawEditor()};document.getElementById('editorCancel').onclick=()=>closeEditor(null);document.getElementById('editorClose').onclick=()=>closeEditor(null);document.getElementById('editorOk').onclick=async()=>{const button=document.getElementById('editorOk');button.disabled=true;const result=await processedEditorFile();button.disabled=false;closeEditor(result)};editorBackdrop.addEventListener('click',e=>{if(e.target===editorBackdrop)closeEditor(null)});document.addEventListener('keydown',e=>{if(e.key==='Escape'&&editorBackdrop.classList.contains('show'))closeEditor(null)});editorCanvas.addEventListener('pointerdown',e=>{
  editorCanvas.setPointerCapture(e.pointerId);
  editorState.pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(editorState.pointers.size===1){
    editorState.dragging=true;
    editorState.lastX=e.clientX;editorState.lastY=e.clientY;
  }else if(editorState.pointers.size===2){
    editorState.dragging=false; // 2 ngón -> chuyển sang pinch, không kéo nữa
    const pts=[...editorState.pointers.values()];
    editorState.pinchStartDist=Math.hypot(pts[0].x-pts[1].x,pts[0].y-pts[1].y);
    editorState.pinchStartScale=editorState.scale;
  }
});
editorCanvas.addEventListener('pointermove',e=>{
  if(!editorState.pointers.has(e.pointerId))return;
  editorState.pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});

  if(editorState.pointers.size===2){
    const pts=[...editorState.pointers.values()];
    const dist=Math.hypot(pts[0].x-pts[1].x,pts[0].y-pts[1].y);
    if(editorState.pinchStartDist>0){
      const nextScale=Math.min(3,Math.max(1,editorState.pinchStartScale*(dist/editorState.pinchStartDist)));
      editorState.scale=nextScale;
      editorZoom.value=nextScale.toFixed(2);
      drawEditor();
    }
    return;
  }

  if(!editorState.dragging)return;
  const rect=editorCanvas.getBoundingClientRect();
  editorState.x+=(e.clientX-editorState.lastX)*editorCanvas.width/rect.width;
  editorState.y+=(e.clientY-editorState.lastY)*editorCanvas.height/rect.height;
  editorState.lastX=e.clientX;editorState.lastY=e.clientY;
  drawEditor();
});
function releasePointer(e){
  editorState.pointers.delete(e.pointerId);
  if(editorState.pointers.size<2)editorState.pinchStartDist=0;
  if(editorState.pointers.size===1){
    // vẫn còn 1 ngón sau khi nhả pinch -> tiếp tục kéo bình thường thay vì dừng hẳn
    const remaining=[...editorState.pointers.values()][0];
    editorState.dragging=true;
    editorState.lastX=remaining.x;editorState.lastY=remaining.y;
  }else{
    editorState.dragging=false;
  }
}
editorCanvas.addEventListener('pointerup',releasePointer);
editorCanvas.addEventListener('pointercancel',releasePointer);
editorCanvas.addEventListener('wheel',e=>{
  e.preventDefault();
  const next=Math.min(3,Math.max(1,editorState.scale+(e.deltaY<0?.1:-.1)));
  editorState.scale=next;
  editorZoom.value=next.toFixed(2);
  drawEditor();
},{passive:false});

async function uploadFiles(fileList){const files=Array.from(fileList||[]);if(!files.length)return;showUploadError('');progress.classList.add('show');document.getElementById('fill').style.width='0%';let errors=[];for(let i=0;i<files.length;i++){const f=files[i];if(!isJpeg(f)){errors.push(uploadMessage('badType','JPG/JPEG only: ')+f.name);continue}let prepared=f;if(editMode){document.getElementById('pct').textContent=files.length>1?`${i+1}/${files.length}`:'Editing…';prepared=await openEditor(f,i,files.length);if(!prepared)break}else{const error=await validateDirect(f);if(error){errors.push(error);continue}}const ok=await uploadSingleFile(prepared,i+1,files.length);if(!ok)errors.push(uploadMessage('failed','Upload failed: ')+f.name)}progress.classList.remove('show');file.value='';if(errors.length)showUploadError(errors.join('\n'));fetchGallery()}
choose.onclick=()=>file.click();['dragenter','dragover'].forEach(e=>drop.addEventListener(e,x=>{x.preventDefault();drop.classList.add('drag')}));['dragleave','drop'].forEach(e=>drop.addEventListener(e,x=>{x.preventDefault();drop.classList.remove('drag')}));drop.addEventListener('drop',e=>uploadFiles(e.dataTransfer.files));file.onchange=e=>uploadFiles(e.target.files);

async function deleteImage(path){
  if(!confirm('Xoá ảnh này khỏi thẻ nhớ?'))return;
  try{
    const r=await fetch('/api/gallery/file?path='+encodeURIComponent(path),{method:'DELETE'});
    if(!r.ok)throw new Error('delete failed');
    fetchGallery();
  }catch(error){
    console.error('deleteImage failed',error);
    alert('Không thể xoá ảnh khỏi thẻ nhớ.');
  }
}

document.querySelectorAll('.reboot').forEach(b=>b.onclick=()=>{b.disabled=true;b.textContent='Rebooting…';fetch('/api/system/reboot',{method:'POST'});setTimeout(()=>{b.disabled=false;b.textContent='↻ Reboot device'},1500)});document.getElementById('factory').onclick=()=>{if(confirm('Factory reset will remove saved settings. Continue?')){fetch('/api/system/factory-reset',{method:'POST'}).then(()=>location.reload())}};

// [NOTE upgrade-resolution] Các chuỗi text bên dưới (uploadHelp, editorHint, badSize trong uploadMessage())
// có nhắc cứng "320 × 240" dưới dạng văn bản hiển thị cho người dùng đọc — khi nâng độ phân giải,
// nhớ sửa luôn các chuỗi text này (cả 2 ngôn ngữ en/vi), vì đây là copy tĩnh, không tự đổi theo
// FRAME_OUTPUT_WIDTH/HEIGHT như phần logic.
const languageData={
  en:{
    pageTitle:'PhotoFrame · ESP32 Smart Photo Frame',languageAria:'Language',navAria:'Primary navigation',sub:'ESP32 control',navLabel:'Workspace',nav:['▦ Dashboard','⌁ WiFi','▧ Gallery','☼ Weather','⚙ Settings'],deviceRow:'Frame online',deviceMeta:'Last sync 2 min ago',eyebrow:'Smart photo frame',
    copy:{dashboard:['Dashboard',"A quick view of your frame's health and display."],wifi:['WiFi','Connect your frame to a nearby network.'],gallery:['Gallery','Manage the photos shown on your frame.'],weather:['Weather','Choose the location shown on your frame.'],settings:['Settings','Personalize the frame and manage device actions.']},
    statNames:['CPU temperature','Free RAM','WiFi signal','SD card'],statNotes:['Normal range','of 512 KB total','Good connection','4.2 GB free'],overviewTitle:'Device overview',overviewText:'Hardware and network details reported by your frame.',badges:['● Live','JPG only'],
    cardTitles:['System information','Screen brightness','Available networks','Network credentials','On frame','SD card','Location settings','Weather preview','Display preferences','Device maintenance'],captions:['Updated just now','Adjust the TFT backlight','Select an SSID to fill the form.','Your password stays on this device.','5 images · sorted by upload','Storage overview','Coordinates are used for the weather API.','Latest frame conditions','Changes apply immediately.','These actions affect the frame.'],
    detailLabels:['ESP32 chip model','Firmware version','Flash size','Uptime','IP address','WiFi RSSI','SD card status','Display'],brightness:'Screen brightness',dim:'Dim',bright:'Bright',reboot:'↻ Reboot device',rebooting:'Rebooting…',wifiText:'WiFi connection',wifiDesc:'Connect the frame to a nearby network.',scan:'⌁ Scan WiFi',available:'Available networks',credentials:'Network credentials',ssidHelp:'Select an SSID to fill the form.',passwordHelp:'Your password stays on this device.',ssidPlaceholder:'Select a network or type one',passwordPlaceholder:'Enter network password',connect:'Connect',resetWiFi:'Reset WiFi',connectedSSID:'Connected SSID',slideInterval:'Slide interval (s)',dropTitle:'Drop JPG photos here',uploadHelp:'Only JPG images with 320 × 240 resolution are supported for the best display quality.',choose:'Upload Images',editMode:'Edit Mode',editorTitle:'Edit image',editorCancel:'Cancel',editorOk:'OK',editorHint:'Drag to position · output 320 × 240 JPEG',uploading:'Uploading photos',onFrame:'On frame',jpgOnly:'JPG only',storage:'Storage overview',storageHelp:'Keep at least 20 MB free for reliable writes.',storageLabels:['Capacity','Used space','Free space','Total images'],weatherText:'Weather',weatherLocation:'Location settings',weatherCoordinates:'Coordinates are used for the weather API.',latitude:'Latitude',longitude:'Longitude',timezone:'Timezone (GMT)',city:'City name',saveLocation:'Save location',weatherPreview:'Weather preview',latestConditions:'Latest frame conditions',currentWeather:'CURRENT WEATHER',partlyCloudy:'Partly cloudy',humidity:'Humidity',updated:'Updated 10:42',settingsDesc:'Personalize the frame and manage device actions.',displayPreferences:'Display preferences',changes:'Changes apply immediately.',darkMode:'Dark mode',darkModeHelp:'Use a darker control panel theme.',screenBrightnessHelp:'Set the TFT backlight level.',maintenance:'Device maintenance',maintenanceHelp:'These actions affect the frame.',factoryReset:'Factory reset',factoryHelp:'Remove saved WiFi, weather, and gallery settings from the device.',factoryConfirm:'Factory reset will remove saved settings. Continue?',scanScanning:'Scanning for nearby networks…',scanFound:'3 networks found just now.',count:n=>`${n} image${n===1?'':'s'} · sorted by upload`,accentTheme:'Accent theme',accentHelp:'Choose the primary interface color.',projectInfo:'Project information',projectCaption:'PhotoFrame frontend and community links.',projectName:'Project name',version:'Version',github:'GitHub',youtube:'YouTube',passwordLabel:'Password',themeOptions:['Orange','Neon pink / purple','Green','Yellow','Red','Blue'],screenBrightnessAria:'Screen brightness',themeAria:'Toggle light and dark mode',darkModeAria:'Toggle dark mode'
  },
  vi:{
    pageTitle:'PhotoFrame · Khung ảnh thông minh ESP32',languageAria:'Ngôn ngữ',navAria:'Điều hướng chính',sub:'Điều khiển ESP32',navLabel:'Không gian làm việc',nav:['▦ Tổng quan','⌁ WiFi','▧ Thư viện','☼ Thời tiết','⚙ Cài đặt'],deviceRow:'Khung ảnh đang kết nối',deviceMeta:'Đồng bộ lần cuối 2 phút trước',eyebrow:'Khung ảnh thông minh',
    copy:{dashboard:['Tổng quan','Xem nhanh tình trạng và màn hình của khung ảnh.'],wifi:['WiFi','Kết nối khung ảnh với mạng gần đây.'],gallery:['Thư viện','Quản lý ảnh hiển thị trên khung ảnh.'],weather:['Thời tiết','Chọn địa điểm hiển thị trên khung ảnh.'],settings:['Cài đặt','Cá nhân hóa khung ảnh và quản lý thiết bị.']},
    statNames:['Nhiệt độ CPU','RAM còn trống','Tín hiệu WiFi','Thẻ SD'],statNotes:['Trong ngưỡng bình thường','trên tổng 512 KB','Kết nối tốt','Còn trống 4.2 GB'],overviewTitle:'Tổng quan thiết bị',overviewText:'Thông tin phần cứng và mạng do khung ảnh báo cáo.',badges:['● Trực tiếp','Chỉ JPG'],
    cardTitles:['Thông tin hệ thống','Độ sáng màn hình','Mạng khả dụng','Thông tin mạng','Trên khung ảnh','Thẻ SD','Cài đặt địa điểm','Xem trước thời tiết','Tùy chọn hiển thị','Bảo trì thiết bị'],captions:['Vừa cập nhật','Điều chỉnh đèn nền TFT','Chọn SSID để điền biểu mẫu.','Mật khẩu được giữ trên thiết bị.','5 ảnh · sắp xếp theo lần tải lên','Tổng quan lưu trữ','Tọa độ được dùng cho API thời tiết.','Điều kiện mới nhất trên khung ảnh','Thay đổi áp dụng ngay.','Các thao tác này ảnh hưởng đến khung ảnh.'],
    detailLabels:['Mẫu chip ESP32','Phiên bản firmware','Dung lượng flash','Thời gian hoạt động','Địa chỉ IP','RSSI WiFi','Trạng thái thẻ SD','Màn hình'],brightness:'Độ sáng màn hình',dim:'Tối',bright:'Sáng',reboot:'↻ Khởi động lại',rebooting:'Đang khởi động lại…',wifiText:'Kết nối WiFi',wifiDesc:'Kết nối khung ảnh với mạng gần đây.',scan:'⌁ Quét WiFi',available:'Mạng khả dụng',credentials:'Thông tin mạng',ssidHelp:'Chọn SSID để điền biểu mẫu.',passwordHelp:'Mật khẩu được giữ trên thiết bị.',ssidPlaceholder:'Chọn mạng hoặc nhập tên mạng',passwordPlaceholder:'Nhập mật khẩu mạng',connect:'Kết nối',resetWiFi:'Đặt lại WiFi',connectedSSID:'SSID đã kết nối',slideInterval:'Khoảng chuyển ảnh (giây)',dropTitle:'Thả ảnh JPG vào đây',uploadHelp:'Chỉ hỗ trợ ảnh JPG độ phân giải 320 × 240 để hiển thị tốt nhất.',choose:'Tải ảnh',editMode:'Chế độ chỉnh sửa',editorTitle:'Chỉnh sửa ảnh',editorCancel:'Hủy',editorOk:'OK',editorHint:'Kéo để căn chỉnh · đầu ra JPEG 320 × 240',uploading:'Đang tải ảnh lên',onFrame:'Trên khung ảnh',jpgOnly:'Chỉ JPG',storage:'Tổng quan lưu trữ',storageHelp:'Giữ trống ít nhất 20 MB để ghi dữ liệu ổn định.',storageLabels:['Dung lượng','Đã sử dụng','Còn trống','Tổng số ảnh'],weatherText:'Thời tiết',weatherLocation:'Cài đặt địa điểm',weatherCoordinates:'Tọa độ được dùng cho API thời tiết.',latitude:'Vĩ độ',longitude:'Kinh độ',timezone:'Múi giờ (GMT)',city:'Tên thành phố',saveLocation:'Lưu địa điểm',weatherPreview:'Xem trước thời tiết',latestConditions:'Điều kiện mới nhất trên khung ảnh',currentWeather:'THỜI TIẾT HIỆN TẠI',partlyCloudy:'Nhiều mây, có nắng',humidity:'Độ ẩm',updated:'Cập nhật 10:42',settingsDesc:'Cá nhân hóa khung ảnh và quản lý thiết bị.',displayPreferences:'Tùy chọn hiển thị',changes:'Thay đổi áp dụng ngay.',darkMode:'Chế độ tối',darkModeHelp:'Dùng giao diện điều khiển tối hơn.',screenBrightnessHelp:'Đặt mức đèn nền TFT.',maintenance:'Bảo trì thiết bị',maintenanceHelp:'Các thao tác này ảnh hưởng đến khung ảnh.',factoryReset:'Khôi phục cài đặt gốc',factoryHelp:'Xóa WiFi, thời tiết và cài đặt thư viện đã lưu trên thiết bị.',factoryConfirm:'Khôi phục cài đặt gốc sẽ xóa các cài đặt đã lưu. Tiếp tục?',scanScanning:'Đang quét các mạng gần đây…',scanFound:'Đã tìm thấy 3 mạng.',count:n=>`${n} ảnh · sắp xếp theo lần tải lên`,accentTheme:'Màu chủ đạo',accentHelp:'Chọn màu chính cho giao diện.',projectInfo:'Thông tin dự án',projectCaption:'Frontend PhotoFrame và các liên kết cộng đồng.',projectName:'Tên dự án',version:'Phiên bản',github:'GitHub',youtube:'YouTube',passwordLabel:'Mật khẩu',themeOptions:['Cam','Hồng neon / tím','Xanh lá','Vàng','Đỏ','Xanh dương'],screenBrightnessAria:'Độ sáng màn hình',themeAria:'Bật/tắt giao diện sáng và tối',darkModeAria:'Bật/tắt chế độ tối'
  }
};
languageData.en.statValues=['38.4°C','182 KB','−58 dBm','Ready'];languageData.vi.statValues=['38.4°C','182 KB','−58 dBm','Sẵn sàng'];languageData.en.ipAddress='IP address';languageData.vi.ipAddress='Địa chỉ IP';languageData.en.sdMounted='Mounted · 16 GB';languageData.vi.sdMounted='Đã gắn · 16 GB';languageData.en.sub='';languageData.vi.sub='';languageData.en.nav=['Dashboard','WiFi','Weather','Settings'];languageData.vi.nav=['Tổng quan','WiFi','Thời tiết','Cài đặt'];languageData.en.scan=' Scan WiFi';languageData.vi.scan=' Quét WiFi';languageData.en.galleryTopDesc='Upload and organize photos for your frame.';languageData.vi.galleryTopDesc='Tải lên và sắp xếp ảnh cho khung ảnh.';languageData.en.timezonePlaceholder='e.g. +07 or -05';languageData.vi.timezonePlaceholder='Ví dụ: +07 hoặc -05';languageData.en.timezoneInvalid='Enter a GMT offset from -14 to +14.';languageData.vi.timezoneInvalid='Nhập múi giờ GMT từ -14 đến +14.';
languageData.en.scan='Scan WiFi';languageData.vi.scan='Quét WiFi';
const dashboardPanel=document.getElementById('dashboard'),galleryPanel=document.getElementById('gallery'),settingsPanel=document.getElementById('settings');
const hardwareSettings=document.createElement('div');hardwareSettings.className='hardwareSettings';
[...dashboardPanel.children].filter(child=>child.matches('.stats,.head,.dash')).forEach(child=>hardwareSettings.appendChild(child));
hardwareSettings.querySelector('.bright')?.remove();const brightValue=document.createElement('span');brightValue.id='brightVal';brightValue.hidden=true;settingsPanel.appendChild(brightValue);
settingsPanel.insertBefore(hardwareSettings,settingsPanel.querySelector('.two'));
hardwareSettings.querySelector('.dash').style.gridTemplateColumns='1fr';
galleryPanel.classList.remove('panel');galleryPanel.classList.add('galleryEmbedded');dashboardPanel.replaceChildren(galleryPanel);
document.querySelector('.nav button[data-tab="gallery"]')?.remove();
const logoSub=document.querySelector('.sub');if(logoSub)logoSub.style.display='none';
document.querySelector('.avatar')?.remove();
const navIconSvg={dashboard:'<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="4" y="4" width="6" height="6" rx="1"/><rect x="14" y="4" width="6" height="6" rx="1"/><rect x="4" y="14" width="6" height="6" rx="1"/><rect x="14" y="14" width="6" height="6" rx="1"/></svg>',wifi:'<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2.5 8.8a14.5 14.5 0 0 1 19 0M5.5 12.2a9.7 9.7 0 0 1 13 0M8.7 15.7a5 5 0 0 1 6.6 0M12 19h.01"/></svg>',weather:'<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="3.5"/><path d="M12 2v2.5M12 19.5V22M4.9 4.9l1.8 1.8M17.3 17.3l1.8 1.8M2 12h2.5M19.5 12H22M4.9 19.1l1.8-1.8M17.3 6.7l1.8-1.8"/></svg>',settings:'<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 8.2a3.8 3.8 0 1 0 0 7.6 3.8 3.8 0 0 0 0-7.6Z"/><path d="m19.4 15 .1.1a1.8 1.8 0 0 1-2.5 2.5l-.1-.1a1.8 1.8 0 0 0-3 .9v.2a1.8 1.8 0 0 1-3.6 0v-.2a1.8 1.8 0 0 0-3-.9l-.1.1a1.8 1.8 0 1 1-2.5-2.5l.1-.1a1.8 1.8 0 0 0-.9-3h-.2a1.8 1.8 0 0 1 0-3.6h.2a1.8 1.8 0 0 0 .9-3l-.1-.1a1.8 1.8 0 1 1 2.5-2.5l.1.1a1.8 1.8 0 0 0 3-.9v-.2a1.8 1.8 0 0 1 3.6 0v.2a1.8 1.8 0 0 0 3 .9l.1-.1a1.8 1.8 0 1 1 2.5 2.5l-.1.1a1.8 1.8 0 0 0 .9 3h.2a1.8 1.8 0 0 1 0 3.6h-.2a1.8 1.8 0 0 0-.9 3Z"/></svg>'};
Object.keys(navIconSvg).forEach(key=>{navIconSvg[key]=navIconSvg[key].replace('<svg ','<svg fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" ')})
function repairNavIcons(t){document.querySelectorAll('.nav button').forEach(button=>{const tab=button.dataset.tab,index={dashboard:0,wifi:1,weather:2,settings:3}[tab];button.innerHTML=navIconSvg[tab]+'<span class="navText">'+t.nav[index]+'</span>'})}
function repairWifiIcons(){document.querySelectorAll('.wifi').forEach(icon=>{icon.innerHTML=navIconSvg.wifi;const svg=icon.firstElementChild;svg.setAttribute('width','20');svg.setAttribute('height','20')})}repairWifiIcons();
const timezoneControl=document.getElementById('tz'),timezoneInput=document.createElement('input');timezoneInput.id='tz';timezoneInput.type='text';timezoneInput.inputMode='decimal';timezoneInput.value='-05';timezoneInput.placeholder='e.g. +07 or -05';timezoneInput.setAttribute('aria-label','Timezone (GMT)');timezoneControl.replaceWith(timezoneInput);loadWeatherSettings();
//Sync Data weather
const weatherCodeText={0:'Clear sky',1:'Mostly clear',2:'Partly cloudy',3:'Overcast',45:'Fog',48:'Fog',51:'Light drizzle',53:'Drizzle',55:'Dense drizzle',61:'Light rain',63:'Rain',65:'Heavy rain',66:'Freezing rain',67:'Freezing rain',71:'Light snow',73:'Snow',75:'Heavy snow',80:'Rain showers',81:'Rain showers',82:'Violent showers',95:'Thunderstorm',96:'Thunderstorm',99:'Thunderstorm'};
const weatherCodeIcon={0:'☼',1:'☼',2:'⛅',3:'☁',45:'▤',48:'▤',51:'☂',53:'☂',55:'☂',61:'☔',63:'☔',65:'☔',66:'☔',67:'☔',71:'❄',73:'❄',75:'❄',80:'☔',81:'☔',82:'☔',95:'⚡',96:'⚡',99:'⚡'};
async function fetchCurrentWeather(){
  try{
    const r=await fetch('/api/weather/current');
    if(!r.ok)return;
    const d=await r.json();
    if(!d.valid)return;
    document.getElementById('weatherTemp').textContent=Math.round(d.temperature)+'°';
    document.getElementById('weatherDesc').textContent=weatherCodeText[d.weatherCode]||'Unknown';
    document.getElementById('weatherIcon').textContent=weatherCodeIcon[d.weatherCode]||'☁';
    document.getElementById('weatherUpdated').textContent='Updated '+(d.timeValid?d.time:'--');
  }catch(e){console.error('Fetch current weather failed',e)}
}
fetchCurrentWeather();
setInterval(fetchCurrentWeather,60000);
async function loadWeatherSettings(){
  try{
    const r = await fetch('/api/weather');
    if(!r.ok) return;
    const cfg = await r.json();
    document.getElementById('lat').value = cfg.latitude;
    document.getElementById('lon').value = cfg.longitude;
    document.getElementById('city').value = cfg.city;
    const hours = cfg.timezoneSeconds / 3600;
    timezoneInput.value = (hours >= 0 ? '+' : '-') + String(Math.abs(hours)).padStart(2,'0');
    validateTimezone();
    document.getElementById('preview').textContent = cfg.city + ' · ' + formatTimezone(hours);
  }catch(e){
    console.error('Load weather config failed', e);
  }
}
function parseTimezoneHours(value){const raw=String(value).trim().replace(/^GMT\s*/i,'');if(!raw)return null;const clock=raw.match(/^([+-]?\d{1,2}):([0-5]\d)$/);const hours=clock?(clock[1].startsWith('-')?-1:1)*(Math.abs(Number(clock[1]))+Number(clock[2])/60):Number(raw);return Number.isFinite(hours)&&hours>=-14&&hours<=14?hours:null}
function formatTimezone(hours){const totalMinutes=Math.round(Math.abs(hours)*60),wholeHours=Math.floor(totalMinutes/60),minutes=totalMinutes%60;return 'GMT '+(hours<0?'-':'+')+String(wholeHours).padStart(2,'0')+':'+String(minutes).padStart(2,'0')}
function validateTimezone(){const hours=parseTimezoneHours(timezoneInput.value),message=languageData[activeLanguage].timezoneInvalid;timezoneInput.setCustomValidity(hours===null?message:'');timezoneInput.dataset.seconds=hours===null?'':String(hours*3600);return hours}
timezoneInput.addEventListener('input',validateTimezone);document.getElementById('saveWeather').onclick=()=>{const hours=validateTimezone();if(hours===null){timezoneInput.reportValidity();return}const seconds=hours*3600;const lat=document.getElementById('lat').value;const lon=document.getElementById('lon').value;const city=document.getElementById('city').value;window.photoFrameWeatherConfig={city:city,lat:lat,lon:lon,timezoneHours:hours,timezoneSeconds:seconds};document.getElementById('preview').textContent=city+' · '+formatTimezone(hours);fetch('/api/weather',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'lat='+encodeURIComponent(lat)+'&lon='+encodeURIComponent(lon)+'&tz='+encodeURIComponent(seconds)+'&city='+encodeURIComponent(city)})};
const languageSelect=document.createElement('select');
languageSelect.className='language';
languageSelect.id='language';
languageSelect.style.cssText='width:auto;height:38px;padding:0 9px;font-size:12px;font-weight:600';
languageSelect.innerHTML='<option value="en">English</option><option value="vi">Tiếng Việt</option>';
document.querySelector('.actions').insertBefore(languageSelect,document.getElementById('theme'));
let activeLanguage=localStorage.getItem('photoFrameLanguage')==='vi'?'vi':'en';validateTimezone();
const setTexts=(selector,values)=>document.querySelectorAll(selector).forEach((node,index)=>{if(values[index]!==undefined)node.textContent=values[index]});
const setLeadingText=(node,value)=>{if(!node)return;const textNode=[...node.childNodes].find(child=>child.nodeType===Node.TEXT_NODE);if(textNode)textNode.nodeValue=value;else node.appendChild(document.createTextNode(value))};
const galleryCount=()=>document.querySelectorAll('.galleryItem').length;
function applyLanguage(){
  const t=languageData[activeLanguage],activeTab=document.querySelector('.nav button.active')?.dataset.tab||'dashboard';
  document.documentElement.lang=activeLanguage;document.title=t.pageTitle;languageSelect.value=activeLanguage;languageSelect.setAttribute('aria-label',t.languageAria);
  document.querySelector('.side').setAttribute('aria-label',t.navAria);document.querySelector('.sub').textContent=t.sub;document.querySelector('.navlabel').textContent=t.navLabel;document.querySelector('.eyebrow').textContent=t.eyebrow;setLeadingText(document.querySelector('.deviceRow'),t.deviceRow);document.querySelector('.deviceMeta').textContent=t.deviceMeta;setTexts('.nav button',t.nav);title.textContent=t.copy[activeTab][0];desc.textContent=t.copy[activeTab][1];
  setTexts('#dashboard .statTop > span:first-child',t.statNames);setTexts('#dashboard .note',t.statNotes);document.querySelector('#dashboard .title').textContent=t.overviewTitle;document.querySelector('#dashboard .subtext').textContent=t.overviewText;setTexts('.badge',t.badges);setTexts('.cardTitle',t.cardTitles);setTexts('.caption',t.captions);setTexts('#dashboard .label',t.detailLabels);setTexts('#dashboard .scale span',[t.dim,t.bright]);
  document.querySelector('#wifi .title').textContent=t.wifiText;setTexts('#wifi label',[t.passwordLabel]);document.getElementById('ssid').placeholder=t.ssidPlaceholder;document.getElementById('pass').placeholder=t.passwordPlaceholder;document.getElementById('scan').textContent=t.scan;document.getElementById('resetWiFi').textContent=t.resetWiFi;document.querySelector('#wifi .subtext').textContent=t.wifiDesc;document.querySelectorAll('#wifi .cardTitle')[0].textContent=t.available;document.querySelectorAll('#wifi .cardTitle')[1].textContent=t.credentials;document.querySelectorAll('#wifi .caption')[0].textContent=t.ssidHelp;document.querySelectorAll('#wifi .caption')[1].textContent=t.passwordHelp;document.getElementById('connect').textContent=t.connect;setTexts('#wifi .connection span',[t.connectedSSID,'IP address']);
  document.querySelector('#gallery .title').textContent=t.copy.gallery[0];document.querySelector('#gallery .subtext').textContent=t.copy.gallery[1];document.querySelector('#gallery label').textContent=t.slideInterval;document.querySelector('.dropTitle').textContent=t.dropTitle;setTexts('#gallery .help',[t.uploadHelp,t.storageHelp]);document.querySelector('.progressMeta span').textContent=t.uploading;document.querySelector('#gallery .cardTitle').textContent=t.onFrame;document.querySelector('#gallery .galleryLayout .cardTitle').textContent=t.storage;document.querySelector('#gallery .badge').textContent=t.jpgOnly;document.getElementById('choose').textContent=t.choose;document.querySelector('.editSetting span').textContent=t.editMode;editToggle.setAttribute('aria-label',t.editMode);document.getElementById('editorTitle').textContent=t.editorTitle;document.getElementById('editorCancel').textContent=t.editorCancel;document.getElementById('editorOk').textContent=t.editorOk;document.getElementById('editorHint').textContent=t.editorHint;setTexts('.storageStats dt',t.storageLabels);document.getElementById('count').textContent=t.count(galleryCount());
  document.querySelector('#weather .title').textContent=t.weatherText;setTexts('#weather label',[t.latitude,t.longitude,t.timezone,t.city]);document.querySelector('#weather .subtext').textContent=t.copy.weather[1];document.querySelectorAll('#weather .cardTitle')[0].textContent=t.weatherLocation;document.querySelectorAll('#weather .cardTitle')[1].textContent=t.weatherPreview;document.querySelectorAll('#weather .caption')[0].textContent=t.weatherCoordinates;document.querySelectorAll('#weather .caption')[1].textContent=t.latestConditions;document.querySelector('.weather .label').textContent=t.currentWeather;document.querySelector('.temp').nextElementSibling.textContent=t.partlyCloudy;const weatherMeta=document.querySelectorAll('.weatherMeta span');setLeadingText(weatherMeta[0],t.humidity);weatherMeta[1].textContent=t.updated;document.getElementById('saveWeather').textContent=t.saveLocation;
  document.querySelector('#settings .title').textContent=t.copy.settings[0];document.querySelector('#settings .subtext').textContent=t.settingsDesc;document.querySelectorAll('#settings .cardTitle')[0].textContent=t.displayPreferences;document.querySelectorAll('#settings .cardTitle')[1].textContent=t.maintenance;document.querySelectorAll('#settings .cardTitle')[2].textContent=t.projectInfo;document.querySelectorAll('#settings .caption')[0].textContent=t.changes;document.querySelectorAll('#settings .caption')[1].textContent=t.maintenanceHelp;document.querySelectorAll('#settings .caption')[2].textContent=t.projectCaption;setTexts('.settingCopy strong',[t.darkMode,t.accentTheme,t.brightness]);setTexts('.settingCopy span',[t.darkModeHelp,t.accentHelp,t.screenBrightnessHelp]);document.querySelector('.dangerZone h3').textContent=t.factoryReset;document.querySelector('.dangerZone p').textContent=t.factoryHelp;document.getElementById('factory').textContent=t.factoryReset;const themeLabels=document.querySelectorAll('#accentTheme option');themeLabels.forEach((option,index)=>{option.textContent=t.themeOptions[index]});document.getElementById('accentTheme').setAttribute('aria-label',t.accentTheme);setTexts('.projectMeta dt',[t.projectName,t.version]);setTexts('.projectLinks a span',[t.github,t.youtube]);
  setTexts('.reboot',[t.reboot,t.reboot]);document.getElementById('theme').setAttribute('aria-label',t.themeAria);document.querySelectorAll('input.range').forEach(input=>input.setAttribute('aria-label',t.screenBrightnessAria));document.getElementById('darkSwitch').setAttribute('aria-label',t.darkModeAria);
  setTexts('.hardwareSettings .value',t.statValues);const mountedDetail=document.querySelectorAll('.hardwareSettings .detail strong')[6];if(mountedDetail)mountedDetail.textContent=t.sdMounted;document.querySelectorAll('#wifi .connection span')[1].textContent=t.ipAddress;setLeadingText(document.querySelectorAll('.weatherMeta span')[0],t.humidity+' ');
  const scanState=document.getElementById('scanState');if(scanState.dataset.state==='scanning')scanState.textContent=t.scanScanning;if(scanState.dataset.state==='found')scanState.textContent=t.scanFound;
}
languageSelect.addEventListener('change',()=>{activeLanguage=languageSelect.value==='vi'?'vi':'en';localStorage.setItem('photoFrameLanguage',activeLanguage);applyLanguage()});
document.querySelectorAll('.nav button').forEach(button=>button.addEventListener('click',applyLanguage));
document.getElementById('scan').addEventListener('click',scanWifi);
document.querySelectorAll('.delete').forEach(button=>button.addEventListener('click',()=>{document.getElementById('count').textContent=languageData[activeLanguage].count(galleryCount())}));
document.querySelectorAll('.reboot').forEach(button=>button.addEventListener('click',()=>{const t=languageData[activeLanguage];button.textContent=t.rebooting;setTimeout(()=>button.textContent=languageData[activeLanguage].reboot,1510)}));
const nativeConfirm=window.confirm.bind(window);window.confirm=message=>nativeConfirm(message==='Factory reset will remove saved settings. Continue?'?languageData[activeLanguage].factoryConfirm:message);
function repairLayoutLanguage(){
  const t=languageData[activeLanguage],activeTab=document.querySelector('.nav button.active')?.dataset.tab||'dashboard';
  setTexts('.nav button',t.nav);repairNavIcons(t);repairWifiIcons();timezoneInput.placeholder=t.timezonePlaceholder;timezoneInput.setAttribute('aria-label',t.timezone);validateTimezone();
  document.querySelector('#gallery .title').textContent=t.copy.gallery[0];document.querySelector('#gallery .subtext').textContent=t.copy.gallery[1];setTexts('#gallery .cardTitle',[t.cardTitles[4],t.cardTitles[5]]);setTexts('#gallery .caption',[t.captions[4],t.captions[5]]);document.querySelector('#gallery .badge').textContent=t.badges[1];document.getElementById('count').textContent=t.count(galleryCount());
  document.querySelector('.hardwareSettings .title').textContent=t.overviewTitle;document.querySelector('.hardwareSettings .subtext').textContent=t.overviewText;setTexts('.hardwareSettings .statTop > span:first-child',t.statNames);setTexts('.hardwareSettings .note',t.statNotes);setTexts('.hardwareSettings .value',t.statValues);setTexts('.hardwareSettings .label',t.detailLabels);setTexts('.hardwareSettings .cardTitle',[t.cardTitles[0]]);setTexts('.hardwareSettings .caption',[t.captions[0]]);document.querySelector('.hardwareSettings .badge').textContent=t.badges[0];document.querySelectorAll('.hardwareSettings .detail strong')[6].textContent=t.sdMounted;
  setTexts('#settings .two .cardTitle',[t.displayPreferences,t.maintenance]);setTexts('#settings .two .caption',[t.changes,t.maintenanceHelp]);setTexts('#settings .projectPanel .cardTitle',[t.projectInfo]);setTexts('#settings .projectPanel .caption',[t.projectCaption]);setTexts('.projectMeta dt',[t.projectName,t.version]);setTexts('.projectLinks a span',[t.github,t.youtube]);
  if(activeTab==='dashboard'){title.textContent=t.copy.dashboard[0];desc.textContent=t.galleryTopDesc}
}
languageSelect.addEventListener('change',repairLayoutLanguage);document.querySelectorAll('.nav button').forEach(button=>button.addEventListener('click',repairLayoutLanguage));
applyLanguage();repairLayoutLanguage();
