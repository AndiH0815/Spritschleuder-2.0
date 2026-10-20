// Tests the actual browser UI with a mock API, not ESP32 hardware or TLS.
const { chromium } = require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES ? process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES + '/playwright' : 'playwright');
const fs = require('fs'),http = require('http'),assert = require('assert');
require('child_process').execFileSync('python3',['tools/embed_web.py','main/web.html','tests/build/web.html.gz']);
const compressedPage=fs.readFileSync('tests/build/web.html.gz');
let logged=false,admin=false,orders=0,heartbeats=0,uploaded=null,lastOrder=null;
let status={boot_id:7,state:'idle',error:'',online:true,busy:false,job:0,outputs:0,elapsed_ms:0,duration_ms:10000,manual_ml:340,admin:false,sequence:0,hardware_enabled:true,photos_ready:true};
let pumps=Array.from({length:8},(_,i)=>({active:i<4,ingredient:i+1,ml_per_second:2}));
let configuration={ingredients:[{id:1,name:'Zutat 1'},{id:4,name:'Limette'}],glasses:[{name:'Klein',ml:200},{name:'Groß',ml:400}]};
let recipes=[{id:1,name:'Cuba Libre',manual:'Cola',photo:'',review:false,available:true,portions:[{ingredient:1,ml:40,strength:true},{ingredient:4,ml:20,strength:false}]}];
let nextId=2;
let network={connected:true,configured:true,setup:false,testing:false,ssid:'Home',saved_ssid:'Home',ip:'192.168.1.50',message:''},lastWifi=null;
// Exercise the real firmware policy rather than a less restrictive test page.
const policySource=fs.readFileSync('main/cocktail_server.cpp','utf8').split('"Content-Security-Policy",')[1].split(');')[0];
const policy=Array.from(policySource.matchAll(/"([^"]*)"/g),m=>m[1]).join('');

const server=http.createServer(async(req,res)=>{
  const path=new URL(req.url,'http://localhost').pathname;
  if(path==='/'){res.setHeader('Content-Type','text/html');res.setHeader('Content-Security-Policy',policy);res.setHeader('Content-Encoding','gzip');return res.end(compressedPage)}
  const chunks=[];for await(const chunk of req)chunks.push(chunk);const raw=Buffer.concat(chunks);
  const data=req.headers['content-type']==='application/json'?JSON.parse(raw.toString()):{};
  const reply=(obj,code=200)=>{res.writeHead(code,{'Content-Type':'application/json'});res.end(JSON.stringify(obj))};
  if(path==='/api/login'){if(data.password!=='test-access')return reply({error:'Falsches Passwort'},401);logged=true;return reply(status)}
  if(!logged)return reply({error:'Bitte anmelden'},401);
  if(path==='/api/status')return reply({...status,admin,wifi_setup:network.setup,wifi:admin?network:undefined});
  if(path==='/api/admin'){admin=data.password==='test-admin';return reply({...status,admin},admin?200:403)}
  if(path==='/api/heartbeat'){heartbeats++;return reply({...status,admin})}
  if(path==='/api/abort'){status={...status,state:'stopping',busy:true};return reply({...status,admin})}
  if(path==='/api/logout'){logged=false;return reply({})}
  if(path==='/api/bar'){if(req.method==='POST'){if(!admin)return reply({error:'Verwaltung erforderlich'},403);configuration=data}return reply(configuration)}
  if(path==='/api/recipes'&&req.method==='GET')return reply(recipes);
  if(path==='/api/order'||path==='/api/test'){
    if(status.busy)return reply({error:'Ausgabe aktiv'},409);
    if(path==='/api/test'&&(!admin||!pumps[data.pump].active))return reply({error:'Pumpe gesperrt'},403);
    orders++;if(path==='/api/order'){lastOrder=data;const recipe=recipes.find(r=>r.id===data.recipe_id);const automatic=recipe.portions.reduce((a,p)=>a+p.ml*data.quantity_ml/400*(p.strength?data.strength:1),0);status.manual_ml=data.quantity_ml-automatic}
    status={...status,state:'running',busy:true,elapsed_ms:0,outputs:1,job:status.job+1,sequence:data.sequence,duration_ms:data.duration_ms||10000};return reply({...status,admin})
  }
  if(!admin)return reply({error:'Verwaltung erforderlich'},403);
  if(path==='/api/wifi'||path==='/api/wifi/start'||path==='/api/wifi/finish'){
    if(status.busy)return reply({error:'Ausgabe aktiv'},409);
    if(req.method==='POST'){
      if(path==='/api/wifi'){lastWifi=data;network={...network,setup:true,testing:true,message:'WLAN-Test läuft'}}
      if(path==='/api/wifi/start')network={...network,setup:true};
      if(path==='/api/wifi/finish')network={...network,setup:false};
    }
    return reply(network);
  }
  if(path==='/api/pumps'){if(req.method==='POST')pumps=data.pumps;return reply(req.method==='POST'?{}:pumps)}
  if(path==='/api/recipes'){let r={...data,id:data.id||nextId++,available:true,review:false,photo:''};const old=recipes.find(x=>x.id===r.id);if(old){r.photo=old.photo;recipes=recipes.map(x=>x.id===r.id?r:x)}else recipes.push(r);return reply(r)}
  if(path==='/api/recipe/delete'){recipes=recipes.filter(r=>r.id!==data.id);return reply({})}
  if(path==='/api/photo/delete'){const r=recipes.find(r=>r.id===data.id);r.photo='';return reply(r)}
  if(path.startsWith('/api/photo/')){const r=recipes.find(r=>r.id===Number(path.split('/').pop()));if(req.method==='POST'){uploaded=raw;r.photo='1234567890abcdef';return reply(r)}res.setHeader('Content-Type','image/jpeg');return res.end(uploaded||Buffer.alloc(0))}
  reply({error:'Unbekannt'},404);
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const browser=await chromium.launch({executablePath:process.env.SPRIT_CHROMIUM,args:['--no-sandbox','--disable-dev-shm-usage'],headless:true});
  try{
    const page=await browser.newPage({viewport:{width:1024,height:1000}}),errors=[];page.setDefaultTimeout(15000);page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>{delete Element.prototype.replaceChildren});
    await page.goto('http://127.0.0.1:'+server.address().port+'/');
    await page.locator('#password').fill('wrong');await page.locator('#loginForm button').click();await page.getByText('Falsches Passwort',{exact:true}).waitFor();
    await page.locator('#password').fill('test-access');await page.locator('#loginForm button').click();await page.locator('#cards button').waitFor();
    await page.locator('#size').selectOption('200');await page.locator('#strength').selectOption('0.5');await page.locator('#cards button').click();
    assert((await page.locator('#selectedAmounts').textContent()).includes('20 ml aus der Maschine'));
    assert((await page.locator('#selectedAmounts').textContent()).includes('180 ml Cola'));
    await page.locator('[data-view=bar]').click();await page.locator('#adminPassword').fill('test-admin');await page.getByRole('button',{name:'Freigeben',exact:true}).click();await page.locator('#recipeForm').waitFor();
    assert.equal(await page.locator('.part').count(),8);
    await page.locator('#recipeName').fill('Party Libre');await page.locator('#manual').fill('Cola');
    await page.locator('#partIngredient0').selectOption('1');await page.locator('#partMl0').fill('40');await page.locator('#partStrength0').check();
    await page.locator('#partIngredient1').selectOption('4');await page.locator('#partMl1').fill('20');await page.locator('#saveRecipe').click();await page.getByText('Rezept gespeichert',{exact:true}).waitFor();
    assert.equal(recipes[1].portions.length,2);assert(recipes[1].portions[0].strength);
    await page.locator('[data-ingredient="1"]').fill('Rum');await page.locator('#glassName1').fill('Partyglas');await page.locator('#glassMl1').fill('350');await page.locator('#saveBar').click();await page.getByText('Zutaten und Glasgrößen gespeichert',{exact:true}).waitFor();
    assert.equal(configuration.ingredients.find(x=>x.id===1).name,'Rum');assert.equal(configuration.glasses[1].ml,350);
    // Browser converts/crops an input PNG into a bounded JPEG for the device.
    await page.locator('#photoFile').setInputFiles({name:'drink.png',mimeType:'image/png',buffer:Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+j3ioAAAAASUVORK5CYII=','base64')});
    await page.locator('#photoCrop').waitFor();await page.locator('#uploadPhoto').click();await page.getByText('Foto auf der Maschine gespeichert',{exact:true}).waitFor();
    assert(uploaded.length<=49152&&uploaded[0]===255&&uploaded[1]===216);
    await page.locator('[data-view=tech]').click();await page.locator('#active7').waitFor();assert.equal(await page.locator('.pump').count(),8);
    await page.locator('#active7').check();await page.locator('#ml_per_second7').fill('1.5');await page.locator('#savePumps').click();await page.getByText('Pumpeneinstellungen gespeichert',{exact:true}).waitFor();assert(pumps[7].active&&pumps[7].ml_per_second===1.5);
    await page.locator('#testPump').selectOption('7');await page.locator('#test').click();await page.locator('#running').waitFor();
    assert(await page.locator('[data-view=guest]').isDisabled());await page.waitForTimeout(700);assert(heartbeats>0&&orders===1);
    status={...status,state:'done',busy:false,outputs:0,elapsed_ms:10000};await page.locator('#calibration').waitFor();await page.locator('#measured').fill('25');await page.locator('#calibrate').click();assert.equal(await page.locator('#ml_per_second7').inputValue(),'2.5000');
    // Saving survives a view change; the guest sees the edited glass profiles.
    await page.locator('#savePumps').click();await page.getByText('Pumpeneinstellungen gespeichert',{exact:true}).waitFor();
    await page.locator('[data-view=guest]').click();await page.locator('#size').selectOption('350');await page.locator('#strength').selectOption('1');await page.locator('[data-recipe="1"]').click();await page.locator('#order').click();await page.locator('#running').waitFor();
    assert.equal(lastOrder.quantity_ml,350);assert.equal(lastOrder.strength,1);
    status.elapsed_ms=status.duration_ms;await page.waitForTimeout(450);assert(await page.locator('[data-view=guest]').isDisabled());assert(await page.locator('#finish').isHidden());
    await page.locator('#stop').click();await page.waitForTimeout(650);assert(status.state==='stopping'&&await page.locator('[data-view=guest]').isDisabled());
    status={...status,state:'aborted',busy:false};await page.locator('#guest').waitFor();assert(await page.locator('#finish').isHidden());
    await page.locator('#order').click();await page.locator('#running').waitFor();status={...status,state:'done',busy:false};await page.locator('#finish').waitFor();assert((await page.locator('#finishText').textContent()).includes('298 ml Cola'));
    // A maintenance run must not present an alcoholic drink as finished.
    await page.locator('#finishOk').click();await page.locator('[data-view=tech]').click();await page.locator('#testMode').selectOption('rinse');await page.locator('#test').click();await page.locator('#running').waitFor();status={...status,state:'done',busy:false};await page.getByText('Lauf abgeschlossen',{exact:true}).waitFor();
    await page.locator('[data-view=guest]').click();assert(await page.locator('#finish').isHidden());
    for(const width of [1024,736,360,320]){
      await page.setViewportSize({width,height:1000});
      for(const target of ['guest','bar','tech']){
        await page.locator('[data-view='+target+']').click();await page.locator('#'+target).waitFor();
        assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Overflow '+width+' '+target);
      }
    }
    // WLAN setup remains in the protected Maintenance view and locks starts.
    await page.locator('[data-view=tech]').click();await page.locator('#wifiSsid').waitFor();
    assert.equal(await page.locator('#wifiSsid').inputValue(),'Home');
    await page.locator('#wifiSsid').fill('New home');await page.locator('#wifiPassword').fill('new-password');
    await page.locator('#wifiSave').click();await page.getByText('WLAN-Test läuft',{exact:true}).waitFor();
    assert.deepEqual(lastWifi,{ssid:'New home',password:'new-password'});
    assert.equal(await page.locator('#wifiPassword').inputValue(),'');
    assert(await page.locator('#wifiSave').isDisabled()&&await page.locator('#test').isDisabled());
    await page.locator('[data-view=guest]').click();await page.locator('[data-recipe="1"]').click();assert(await page.locator('#order').isDisabled());
    network={...network,testing:false,message:'WLAN-Test fehlgeschlagen; bisherige Daten erhalten'};
    await page.locator('[data-view=tech]').click();await page.getByText('WLAN-Test fehlgeschlagen; bisherige Daten erhalten',{exact:true}).waitFor();
    assert(await page.locator('#wifiSave').isEnabled());
    network={...network,ssid:'New home',saved_ssid:'New home',ip:'192.168.1.60',setup:false,message:'Mit WLAN verbunden'};
    await page.getByText('Mit WLAN verbunden',{exact:true}).waitFor();assert(await page.locator('#test').isEnabled());
    assert.equal(await page.locator('#wifiLink').getAttribute('href'),'https://192.168.1.60');
    for(const width of [1024,736,360,320]){await page.setViewportSize({width,height:1000});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'WiFi form overflow '+width)}
    // Expired elevated access hides administration without exposing its forms.
    admin=false;await page.locator('#adminLogin').waitFor();assert(await page.locator('#tech').isHidden());
    assert.deepEqual(errors,[]);console.log('Guest doses, three menus, recipes, stable names, glass profiles, JPEG upload, 8 slots, calibration, heartbeat, stop confirmation, maintenance completion, protected WiFi setup, password clearing, setup start lock and responsive layouts: PASS');
  }finally{await browser.close();server.close()}
})().catch(e=>{console.error(e);server.close();process.exitCode=1});
