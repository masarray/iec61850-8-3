#include "workbench_page.hpp"

namespace ar61850::dms {

std::string_view workbench_page_html() noexcept {
    static constexpr std::string_view page = R"AR61850HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AR61850 DMS Workbench</title>
<style>
:root{
  color-scheme:light;
  --bg:#f5f7fa;
  --panel:#ffffff;
  --line:#d9e0e7;
  --line2:#e9edf2;
  --text:#17212b;
  --muted:#667482;
  --soft:#eef3f7;
  --accent:#2f6fed;
  --good:#14804a;
  --warn:#9a6700;
  --bad:#b42318;
  --rx:#3859c7;
  --tx:#0c7a63;
  --shadow:0 1px 2px rgba(17,24,39,.05);
  --mono:"Cascadia Code","SFMono-Regular",Consolas,monospace;
}
*{box-sizing:border-box}
html,body{height:100%;margin:0;background:var(--bg);color:var(--text);font:13px/1.45 Inter,Segoe UI,system-ui,sans-serif}
button,input{font:inherit}
button{border:1px solid var(--line);background:#fff;color:var(--text);border-radius:6px;padding:6px 9px;cursor:pointer}
button:hover{border-color:#b9c5d1;background:#f9fbfd}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.danger{color:var(--bad)}
input{border:1px solid var(--line);border-radius:6px;background:#fff;padding:6px 8px;color:var(--text);outline:none}
input:focus{border-color:#8fb0f8;box-shadow:0 0 0 2px rgba(47,111,237,.08)}
.app{height:100%;display:grid;grid-template-rows:42px 1fr}
.topbar{display:flex;align-items:center;gap:14px;padding:0 14px;background:#fff;border-bottom:1px solid var(--line);box-shadow:var(--shadow)}
.brand{font-weight:600;letter-spacing:.1px;white-space:nowrap}
.brand span{color:var(--muted);font-weight:500}
.statusbar{display:flex;align-items:center;gap:10px;margin-left:auto;min-width:0}
.pill{display:inline-flex;align-items:center;gap:6px;padding:3px 7px;border:1px solid var(--line);border-radius:999px;background:#fff;color:var(--muted);font-size:12px}
.dot{width:7px;height:7px;border-radius:50%;background:#98a2b3}
.dot.good{background:#22a06b}.dot.warn{background:#d4a72c}.dot.bad{background:#e5484d}
.shell{min-height:0;display:grid;grid-template-columns:172px minmax(0,1fr)}
.nav{background:#fff;border-right:1px solid var(--line);padding:10px 8px;display:flex;flex-direction:column;gap:3px}
.nav button{border:none;background:transparent;text-align:left;padding:7px 9px;border-radius:5px;color:#344054}
.nav button.active{background:#edf3ff;color:#174ea6;font-weight:600}
.nav .section{margin:12px 9px 4px;color:#98a2b3;font-size:10px;text-transform:uppercase;letter-spacing:.08em}
.main{min-width:0;min-height:0;overflow:hidden}
.view{height:100%;display:none}.view.active{display:block}
.server{padding:18px;overflow:auto}
.grid2{display:grid;grid-template-columns:minmax(0,1.2fr) minmax(280px,.8fr);gap:14px}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:7px;box-shadow:var(--shadow)}
.panel-h{display:flex;align-items:center;gap:8px;padding:9px 11px;border-bottom:1px solid var(--line2);font-weight:600}
.panel-b{padding:11px}
.kv{display:grid;grid-template-columns:150px 1fr;gap:7px 14px}
.kv .k{color:var(--muted)}
.code{font-family:var(--mono);font-size:12px}
.toolbar{display:flex;align-items:center;gap:7px;flex-wrap:wrap}
.note{color:var(--muted)}
.engineering{height:100%;display:grid;grid-template-rows:42px 1fr;min-height:0}
.subbar{display:flex;align-items:center;gap:8px;background:#fff;border-bottom:1px solid var(--line);padding:6px 10px}
.subbar input{width:min(340px,40vw)}
.split{min-height:0;display:grid;grid-template-columns:330px minmax(0,1fr)}
.treepane{min-height:0;overflow:auto;background:#fff;border-right:1px solid var(--line)}
.treehead{position:sticky;top:0;z-index:1;background:#fff;padding:8px 10px;border-bottom:1px solid var(--line2);color:var(--muted);font-size:12px}
.tree{padding:5px}
.tree-row{display:flex;align-items:center;gap:5px;padding:3px 6px;border-radius:4px;cursor:pointer;white-space:nowrap}
.tree-row:hover{background:#f5f8fb}
.tree-row.sel{background:#eaf1ff;color:#164aa5}
.twisty{width:13px;color:#7a8997;text-align:center}.node-icon{width:18px;color:#667482;font-family:var(--mono);font-size:10px}
.indent{display:inline-block}
.workspace{min-width:0;min-height:0;background:var(--bg);display:grid;grid-template-rows:auto 1fr}
.detailhead{padding:10px 12px;background:#fff;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:10px}
.detailhead .title{font-weight:600}.detailhead .ref{font:12px var(--mono);color:var(--muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tablewrap{overflow:auto;min-height:0}
table{width:100%;border-collapse:collapse;background:#fff}
th{position:sticky;top:0;background:#f8fafc;color:#667482;font-size:11px;text-align:left;font-weight:600;border-bottom:1px solid var(--line);padding:7px 8px}
td{border-bottom:1px solid var(--line2);padding:6px 8px;vertical-align:middle}
tr:hover td{background:#fbfcfd}
.mono{font-family:var(--mono);font-size:12px}
.fc{display:inline-block;min-width:28px;text-align:center;border:1px solid #cfd8e3;border-radius:4px;padding:1px 4px;background:#f8fafc;font:10px var(--mono);color:#536271}
.value{font-family:var(--mono)}
.value.good{color:var(--good)}.value.null{color:#98a2b3}
.mini-input{width:110px;font-family:var(--mono);padding:4px 6px}
.inspector{height:100%;display:grid;grid-template-rows:42px 1fr;min-height:0}
.tracewrap{overflow:auto;background:#fff}
.dir{font:10px var(--mono);font-weight:700}.dir.RX{color:var(--rx)}.dir.TX{color:var(--tx)}
.empty{padding:30px;color:var(--muted);text-align:center}
.right{margin-left:auto}
.small{font-size:11px}
.badge{display:inline-block;border-radius:4px;padding:1px 5px;background:#eef2f6;color:#536271;font-size:11px}
@media(max-width:900px){.shell{grid-template-columns:56px 1fr}.nav button{font-size:0}.nav button::first-letter{font-size:13px}.nav .section{display:none}.split{grid-template-columns:260px 1fr}.grid2{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="app">
  <header class="topbar">
    <div class="brand">AR61850 <span>/ DMS Workbench</span></div>
    <div class="statusbar">
      <span class="pill"><span id="runDot" class="dot"></span><span id="runText">Runtime</span></span>
      <span class="pill"><span id="transportDot" class="dot"></span><span id="transportText">Transport</span></span>
      <span class="pill"><span id="peerDot" class="dot"></span><span id="peerText">Peer</span></span>
      <span class="pill small">Dropped <b id="dropCount">0</b></span>
    </div>
  </header>
  <div class="shell">
    <nav class="nav">
      <button class="active" data-view="server">Server</button>
      <button data-view="model">Model</button>
      <button data-view="signals">Signals</button>
      <button data-view="inspector">Inspector</button>
      <div class="section">Lab</div>
      <button id="reloadBtn">Refresh</button>
    </nav>
    <main class="main">
      <section id="view-server" class="view active server">
        <div class="grid2">
          <div class="panel">
            <div class="panel-h">Native DMS Server</div>
            <div class="panel-b">
              <div class="kv">
                <div class="k">Runtime</div><div id="svRuntime">—</div>
                <div class="k">Transport</div><div id="svTransport">—</div>
                <div class="k">Peer state</div><div id="svPeer">—</div>
                <div class="k">Model</div><div class="code">IED1 / LD0</div>
                <div class="k">Profile</div><div class="code">iec61850-tpaa-ber-v1</div>
                <div class="k">Trace events</div><div id="svTrace">0</div>
              </div>
              <div class="toolbar" style="margin-top:14px">
                <button id="startTransport" class="primary">Start transport</button>
                <button id="stopTransport">Stop transport</button>
                <button id="refreshServer">Refresh</button>
              </div>
            </div>
          </div>
          <div class="panel">
            <div class="panel-h">Architecture state</div>
            <div class="panel-b note">
              Protocol state stays in the native C++ process. This browser is only an observer and control surface.
              Closing or refreshing this page does not own the IEC association, model, worker, or transport lifecycle.
            </div>
          </div>
        </div>
      </section>

      <section id="view-model" class="view engineering">
        <div class="subbar">
          <input id="treeSearch" placeholder="Filter LD, LN, DO or DA reference">
          <span id="modelStats" class="note small"></span>
          <button class="right" id="modelRefresh">Refresh model</button>
        </div>
        <div class="split">
          <div class="treepane">
            <div class="treehead">IEC 61850 object tree</div>
            <div id="modelTree" class="tree"></div>
          </div>
          <div class="workspace">
            <div class="detailhead">
              <span id="detailTitle" class="title">Select an object</span>
              <span id="detailRef" class="ref"></span>
            </div>
            <div class="tablewrap">
              <table>
                <thead><tr><th>Name</th><th>FC</th><th>Type</th><th>Value</th><th>Reference</th></tr></thead>
                <tbody id="detailRows"></tbody>
              </table>
            </div>
          </div>
        </div>
      </section>

      <section id="view-signals" class="view engineering">
        <div class="subbar">
          <input id="signalSearch" placeholder="Search signal reference, FC or type">
          <span id="signalCount" class="note small"></span>
          <button class="right" id="signalRefresh">Refresh values</button>
        </div>
        <div class="tablewrap">
          <table>
            <thead><tr><th>Reference</th><th>FC</th><th>Type</th><th>Value</th><th>Write</th></tr></thead>
            <tbody id="signalRows"></tbody>
          </table>
        </div>
      </section>

      <section id="view-inspector" class="view inspector">
        <div class="subbar">
          <input id="traceSearch" placeholder="Filter service, direction, associate ID">
          <label class="small note"><input id="autoFollow" type="checkbox" checked> auto follow</label>
          <span id="traceCount" class="note small"></span>
          <button class="right" id="clearTrace">Clear</button>
        </div>
        <div id="traceWrap" class="tracewrap">
          <table>
            <thead><tr><th>#</th><th>Time</th><th>Dir</th><th>Service</th><th>Invoke</th><th>Associate</th><th>Bytes</th></tr></thead>
            <tbody id="traceRows"></tbody>
          </table>
        </div>
      </section>
    </main>
  </div>
</div>
<script>
const S={health:null,model:null,signals:[],traces:[],lastSeq:0,selected:null,view:'server'};
const $=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));
const j=async(url,opt={})=>{
  const r=await fetch(url,{cache:'no-store',...opt});
  const t=await r.text();
  let body; try{body=t?JSON.parse(t):{}}catch{body={error:t||('HTTP '+r.status)}}
  if(!r.ok) throw new Error(body.error||('HTTP '+r.status));
  return body;
};
function setDot(id,state){$(id).className='dot '+(state===true?'good':state===false?'bad':'warn')}
function renderHealth(){
  const h=S.health||{};
  setDot('runDot',!!h.runtimeRunning);$('runText').textContent=h.runtimeRunning?'Runtime running':'Runtime stopped';
  setDot('transportDot',!!h.transportActive);$('transportText').textContent=h.transportActive?'Transport active':'Transport idle';
  setDot('peerDot',h.transportConnected?true:null);$('peerText').textContent=h.transportConnected?'Peer connected':'No peer';
  $('dropCount').textContent=h.droppedMessages??0;
  $('svRuntime').textContent=h.runtimeRunning?'Running':'Stopped';
  $('svTransport').textContent=h.transportActive?'Active':'Stopped';
  $('svPeer').textContent=h.transportConnected?'Connected':'Waiting / disconnected';
  $('svTrace').textContent=h.traceCount??S.traces.length;
  $('startTransport').disabled=!!h.transportActive;
  $('stopTransport').disabled=!h.transportActive;
}
async function refreshHealth(){try{S.health=await j('/api/health');renderHealth()}catch(e){S.health={};renderHealth()}}
function scalarText(v){
  if(v===null||v===undefined)return '—';
  if(typeof v==='object'){
    if('epochMs'in v)return new Date(v.epochMs).toISOString()+(v.clockNotSynchronized?'  [UNSYNC]':'');
    if('validity'in v)return 'validity='+v.validity+', source='+v.source+(v.test?', test':'')+(v.operatorBlocked?', blocked':'');
    return JSON.stringify(v);
  }
  return String(v);
}
function flattenAttrs(attrs,out=[]){for(const a of attrs||[]){if((a.children||[]).length)flattenAttrs(a.children,out);else out.push(a)}return out}
function flattenObjects(objects,out=[]){for(const o of objects||[]){out.push(o);flattenAttrs(o.dataAttributes,out);flattenObjects(o.dataObjects,out)}return out}
function allSignals(model){
  const out=[];
  for(const ld of model?.logicalDevices||[])for(const ln of ld.logicalNodes||[]){
    const walk=o=>{
      for(const a of o.dataAttributes||[]){
        const wa=x=>{if((x.children||[]).length)for(const c of x.children)wa(c);else out.push(x)};
        wa(a);
      }
      for(const c of o.dataObjects||[])walk(c);
    };
    for(const o of ln.dataObjects||[])walk(o);
  }
  return out;
}
function countModel(m){
  let ld=0,ln=0,dobj=0,da=0;
  for(const d of m?.logicalDevices||[]){ld++;for(const n of d.logicalNodes||[]){ln++;const wo=o=>{dobj++;const wa=a=>{da++;for(const c of a.children||[])wa(c)};for(const a of o.dataAttributes||[])wa(a);for(const c of o.dataObjects||[])wo(c)};for(const o of n.dataObjects||[])wo(o)}}
  return {ld,ln,dobj,da};
}
function treeRow(label,ref,kind,depth,node){
  const row=document.createElement('div');row.className='tree-row';row.dataset.ref=ref;row.style.paddingLeft=(6+depth*13)+'px';
  row.innerHTML='<span class="node-icon">'+esc(kind)+'</span><span>'+esc(label)+'</span>';
  row.onclick=()=>selectNode(ref,node,row);return row;
}
function renderTree(){
  const root=$('modelTree');root.innerHTML='';
  const q=$('treeSearch').value.trim().toLowerCase();
  const matches=(label,ref)=>!q||(label+' '+ref).toLowerCase().includes(q);
  for(const ld of S.model?.logicalDevices||[]){
    const ldref=ld.name;if(matches(ld.name,ldref))root.appendChild(treeRow(ld.name,ldref,'LD',0,ld));
    for(const ln of ld.logicalNodes||[]){
      if(matches(ln.name,ln.ref))root.appendChild(treeRow(ln.name,ln.ref,'LN',1,ln));
      const walk=(o,d)=>{
        if(matches(o.name,o.ref))root.appendChild(treeRow(o.name,o.ref,'DO',d,o));
        for(const a of o.dataAttributes||[]){
          const wa=(x,ad)=>{if(matches(x.name,x.ref))root.appendChild(treeRow(x.name,x.ref,'DA',ad,x));for(const c of x.children||[])wa(c,ad+1)};
          wa(a,d+1);
        }
        for(const c of o.dataObjects||[])walk(c,d+1);
      };
      for(const o of ln.dataObjects||[])walk(o,2);
    }
  }
  const stats=countModel(S.model);$('modelStats').textContent=`${stats.ld} LD · ${stats.ln} LN · ${stats.dobj} DO · ${stats.da} DA`;
}
function detailLeaves(node){
  if(node?.dataAttributes)return flattenAttrs(node.dataAttributes,[]);
  if(node?.children&&node?.type)return flattenAttrs([node],[]);
  return [];
}
function selectNode(ref,node,row){
  S.selected={ref,node};document.querySelectorAll('.tree-row.sel').forEach(x=>x.classList.remove('sel'));row?.classList.add('sel');
  $('detailTitle').textContent=node.name||ref;$('detailRef').textContent=ref;
  const rows=detailLeaves(node);$('detailRows').innerHTML=rows.length?rows.map(a=>`<tr><td>${esc(a.name)}</td><td><span class="fc">${esc(a.fc||'')}</span></td><td>${esc(a.type||'')}</td><td class="value">${esc(scalarText(a.value))}</td><td class="mono">${esc(a.ref||'')}</td></tr>`).join(''):'<tr><td colspan="5" class="empty">Select a data object or data attribute to inspect values.</td></tr>';
}
function renderSignals(){
  const q=$('signalSearch').value.trim().toLowerCase();
  const rows=S.signals.filter(a=>!q||(a.ref+' '+a.fc+' '+a.type).toLowerCase().includes(q));
  $('signalCount').textContent=`${rows.length} / ${S.signals.length} leaf signals`;
  $('signalRows').innerHTML=rows.map(a=>{
    const editable=a.type==='float32';
    return `<tr><td class="mono">${esc(a.ref)}</td><td><span class="fc">${esc(a.fc)}</span></td><td>${esc(a.type)}</td><td class="value">${esc(scalarText(a.value))}</td><td>${editable?`<input class="mini-input" data-ref="${esc(a.ref)}" value="${esc(a.value)}"><button data-write="${esc(a.ref)}">Write</button>`:'<span class="note">read-only</span>'}</td></tr>`;
  }).join('')||'<tr><td colspan="5" class="empty">No matching signals.</td></tr>';
  document.querySelectorAll('[data-write]').forEach(b=>b.onclick=async()=>{
    const ref=b.dataset.write;const inp=document.querySelector(`input[data-ref="${CSS.escape(ref)}"]`);
    try{await j('/api/signals/float?ref='+encodeURIComponent(ref)+'&value='+encodeURIComponent(inp.value),{method:'POST'});await refreshModel(false)}
    catch(e){alert(e.message)}
  });
}
async function refreshModel(render=true){
  try{S.model=await j('/api/model');S.signals=allSignals(S.model);if(render){renderTree();renderSignals();if(S.selected){const found=S.signals.find(x=>x.ref===S.selected.ref);if(found)$('detailRows').innerHTML=`<tr><td>${esc(found.name)}</td><td><span class="fc">${esc(found.fc)}</span></td><td>${esc(found.type)}</td><td class="value">${esc(scalarText(found.value))}</td><td class="mono">${esc(found.ref)}</td></tr>`}}else{renderSignals()}}
  catch(e){}
}
function renderTraces(){
  const q=$('traceSearch').value.trim().toLowerCase();
  const rows=S.traces.filter(t=>!q||((t.service||'')+' '+(t.direction||'')+' '+(t.associateId||'')).toLowerCase().includes(q));
  $('traceCount').textContent=`${rows.length} shown · last #${S.lastSeq||0}`;
  $('traceRows').innerHTML=rows.map(t=>`<tr><td class="mono">${t.sequence}</td><td class="mono">${new Date(t.epochMs).toLocaleTimeString()}</td><td><span class="dir ${esc(t.direction)}">${esc(t.direction)}</span></td><td>${esc(t.service)}</td><td class="mono">${t.invokeId??'—'}</td><td class="mono">${esc(t.associateId||'—')}</td><td class="mono">${t.bytes}</td></tr>`).join('')||'<tr><td colspan="7" class="empty">No protocol traffic captured yet.</td></tr>';
  if($('autoFollow').checked){const w=$('traceWrap');w.scrollTop=w.scrollHeight}
}
async function pollTraces(){
  try{
    const n=await j('/api/traces?after='+S.lastSeq+'&limit=500');
    if(Array.isArray(n)&&n.length){S.traces.push(...n);if(S.traces.length>4000)S.traces=S.traces.slice(-4000);S.lastSeq=n[n.length-1].sequence;renderTraces()}
  }catch(e){}
}
async function post(path){try{await j(path,{method:'POST'});await refreshHealth()}catch(e){alert(e.message)}}
document.querySelectorAll('.nav button[data-view]').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.nav button[data-view]').forEach(x=>x.classList.toggle('active',x===b));
  document.querySelectorAll('.view').forEach(x=>x.classList.remove('active'));S.view=b.dataset.view;$('view-'+S.view).classList.add('active');
  if(S.view==='model'||S.view==='signals')refreshModel(true);if(S.view==='inspector')renderTraces();
});
$('treeSearch').oninput=renderTree;$('signalSearch').oninput=renderSignals;$('traceSearch').oninput=renderTraces;
$('startTransport').onclick=()=>post('/api/runtime/transport/start');
$('stopTransport').onclick=()=>post('/api/runtime/transport/stop');
$('refreshServer').onclick=refreshHealth;$('reloadBtn').onclick=()=>{refreshHealth();refreshModel(true);pollTraces()};
$('modelRefresh').onclick=()=>refreshModel(true);$('signalRefresh').onclick=()=>refreshModel(true);
$('clearTrace').onclick=async()=>{await post('/api/traces/clear');S.traces=[];S.lastSeq=0;renderTraces()};
refreshHealth();refreshModel(true);pollTraces();
setInterval(refreshHealth,1000);setInterval(()=>{if(S.view==='model'||S.view==='signals')refreshModel(false)},1500);setInterval(pollTraces,500);
</script>
</body>
</html>
)AR61850HTML";
    return page;
}

} // namespace ar61850::dms
