function render(settings) {
  var initial = JSON.stringify(settings).replace(/</g, "\\u003c");
  return '<!doctype html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>Trein Configuration</title><style>' +
    'body{font:16px sans-serif;max-width:520px;margin:auto;padding:16px;color:#111}' +
    'label{display:block;font-weight:bold;margin:18px 0 6px}' +
    'input,select,button{box-sizing:border-box;width:100%;padding:10px;font:inherit}' +
    'button{margin-top:14px;color:#fff;background:#003082;border:0;border-radius:4px}' +
    '.fav{display:flex;align-items:center;gap:8px;margin-top:8px}.fav span{flex:1}' +
    '.fav button{width:auto;margin:0;background:#c00;padding:6px 10px}' +
    '.hint{color:#555;font-size:13px}[hidden]{display:none}' +
    '@media(prefers-color-scheme:dark){body{color:#eee;background:#121212}' +
    'input,select{color:#eee;background:#292929;border:1px solid #666}.hint{color:#bbb}}' +
    '</style></head><body><h1>Trein</h1>' +
    '<label for="source">Railway network</label><select id="source">' +
    '<option value="ns">Netherlands — NS</option>' +
    '<option value="irail">Belgium — iRail/NMBS/SNCB</option></select>' +
    '<div id="ns"><label for="key">NS API key</label>' +
    '<input id="key" type="text" autocomplete="off" placeholder="Ocp-Apim subscription key">' +
    '<p class="hint">Get a key from apiportal.ns.nl. iRail does not require a key.</p></div>' +
    '<label for="station">Favourite stations</label>' +
    '<p class="hint">Choose up to five destinations. Belgian destinations are selected from favourites on the watch.</p>' +
    '<select id="station"><option value="">Loading stations…</option></select>' +
    '<button id="add" type="button">Add favourite</button><div id="favs"></div>' +
    '<button id="save" type="button">Save</button><script>(function(){' +
    'var init=' + initial + ',favs=init.favourites||[],stations=[];' +
    'var source=document.getElementById("source"),key=document.getElementById("key"),' +
    'station=document.getElementById("station");source.value=init.data_source||"ns";key.value=init.api_key||"";' +
    'function renderFavs(){var box=document.getElementById("favs");box.innerHTML="";' +
    'favs.forEach(function(f,i){var row=document.createElement("div");row.className="fav";' +
    'var name=document.createElement("span");name.textContent=f.name;var remove=document.createElement("button");' +
    'remove.type="button";remove.textContent="Remove";remove.onclick=function(){favs.splice(i,1);renderFavs()};' +
    'row.appendChild(name);row.appendChild(remove);box.appendChild(row)})}' +
    'function renderStations(){station.innerHTML="<option value=\\"\\">Select a station…</option>";' +
    'stations.forEach(function(s){var option=document.createElement("option");option.value=s.code;' +
    'option.textContent=s.name;station.appendChild(option)})}' +
    'function request(url,headers,done){var xhr=new XMLHttpRequest();xhr.open("GET",url,true);' +
    'Object.keys(headers||{}).forEach(function(n){xhr.setRequestHeader(n,headers[n])});' +
    'xhr.onload=function(){if(xhr.status>=200&&xhr.status<300){try{done(null,JSON.parse(xhr.responseText))}' +
    'catch(e){done(e)}}else done(new Error("HTTP "+xhr.status))};xhr.onerror=function(){done(new Error("Network error"))};xhr.send()}' +
    'function load(){var isIRail=source.value==="irail";stations=[];document.getElementById("ns").hidden=isIRail;' +
    'station.innerHTML="<option>Loading stations…</option>";if(!isIRail&&!key.value){station.innerHTML="<option>Enter an NS API key first</option>";return}' +
    'var url=isIRail?"https://api.irail.be/stations/?format=json&lang=nl":' +
    '"https://gateway.apiportal.ns.nl/reisinformatie-api/api/v2/stations";' +
    'var headers=isIRail?{}:{"Ocp-Apim-Subscription-Key":key.value};request(url,headers,function(error,data){' +
    'if(error){station.innerHTML="<option>Could not load stations</option>";return}' +
    'var list=isIRail?(data.station||[]):(data.payload||[]);stations=list.map(function(s){return isIRail?' +
    '{code:s.id,name:s.name||s.standardname}:{code:s.code,name:s.namen?s.namen.middel:s.name}})' +
    '.filter(function(s){return s.code&&s.name});stations.sort(function(a,b){return a.name.localeCompare(b.name)});renderStations()})}' +
    'document.getElementById("add").onclick=function(){if(!station.value||favs.length>=5)return;' +
    'var selected=stations.filter(function(s){return s.code===station.value})[0];' +
    'if(!selected||favs.some(function(f){return f.code===selected.code}))return;favs.push(selected);renderFavs()};' +
    'source.onchange=function(){favs=[];renderFavs();load()};key.onchange=load;' +
    'document.getElementById("save").onclick=function(){var result={data_source:source.value,api_key:key.value,favourites:favs};' +
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(result))};renderFavs();load()' +
    '}())<\/script></body></html>';
}

module.exports = {render: render};
