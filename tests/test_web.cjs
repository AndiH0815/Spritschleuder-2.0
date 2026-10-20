// Browser smoke test against a mock API; not an ESP32 or TLS integration test.
const { chromium } = require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES ? process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES + '/playwright' : 'playwright');
const fs = require('fs');
const http = require('http');
const assert = require('assert');
let logged=false,admin=false,orders=0,heartbeats=0;
let status={boot_id:7,state:'idle',error:'',online:true,busy:false,job:0,outputs:0,elapsed_ms:0,duration_ms:10000,manual_ml:340,admin:false,sequence:0,hardware_enabled:true};
let pumps=Array.from({length:8},(_,i)=>({active:i<4,ingredient:i+1,ml_per_second:2}));
const recipes=[{id:1,name:'Cuba Libre',manual:'Cola',review:false,available:true,portions:[{ingredient:1,ml:40,strength:true},{ingredient:4,ml:20,strength:false}]}];
const server=http.createServer(async(req,res)=>{
  if(req.url==='/'){res.setHeader('Content-Type','text/html');return res.end(fs.readFileSync('main/web.html'));}
  let body='';for await(const chunk of req)body+=chunk;const data=body?JSON.parse(body):{};
  const reply=(obj,code=200)=>{res.writeHead(code,{'Content-Type':'application/json'});res.end(JSON.stringify(obj))};
  if(req.url==='/api/login'){if(data.password!=='test-access')return reply({error:'Falsches Passwort'},401);logged=true;return reply(status)}
  if(!logged)return reply({error:'Bitte anmelden'},401);
  if(req.url==='/api/status')return reply({...status,admin});
  if(req.url==='/api/recipes')return reply(recipes);
  if(req.url==='/api/admin'){admin=data.password==='test-admin';return reply({...status,admin},admin?200:403)}
  if(req.url==='/api/pumps'){if(!admin)return reply({error:'Verwaltung erforderlich'},403);if(req.method==='POST')pumps=data.pumps;return reply(pumps)}
  if(req.url==='/api/test'||req.url==='/api/order'){orders++;status={...status,state:'running',busy:true,job:status.job+1,sequence:data.sequence};return reply({...status,admin})}
  if(req.url==='/api/heartbeat'){heartbeats++;return reply({...status,admin})}
  if(req.url==='/api/abort'){status={...status,state:'stopping',busy:true};return reply({...status,admin})}
  if(req.url==='/api/logout'){logged=false;return reply({})}
  reply({});
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const browser=await chromium.launch({executablePath:process.env.SPRIT_CHROMIUM,args:['--no-sandbox','--disable-dev-shm-usage','--disable-gpu','--no-zygote','--single-process'],headless:true});
  try{
    const page=await browser.newPage({viewport:{width:1024,height:900}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.locator('#password').fill('wrong');await page.locator('#loginForm button').click();await page.getByText('Falsches Passwort',{exact:true}).waitFor();
    await page.locator('#password').fill('test-access');await page.locator('#loginForm button').click();await page.locator('#cards button').waitFor();
    await page.getByRole('button',{name:'Technik',exact:true}).click();await page.locator('#adminPassword').fill('test-admin');await page.locator('#adminForm button').click();await page.locator('#active7').waitFor();assert.equal(await page.locator('.pump').count(),8);
    await page.locator('#active7').check();await page.locator('#ml_per_second7').fill('1.5');await page.locator('#savePumps').click();await page.getByText('Pumpeneinstellungen gespeichert',{exact:true}).waitFor();assert(pumps[7].active&&pumps[7].ml_per_second===1.5);
    await page.locator('#testPump').selectOption('7');await page.locator('#test').click();await page.locator('#running').waitFor();assert(await page.getByRole('button',{name:'Drinks',exact:true}).isDisabled());
    await page.waitForTimeout(600);assert(heartbeats>0&&orders===1);
    status={...status,state:'done',busy:false,outputs:0,elapsed_ms:10000};await page.locator('#measured').waitFor();await page.locator('#measured').fill('25');await page.locator('#calibrate').click();assert.equal(await page.locator('#ml_per_second7').inputValue(),'2.5000');
    await page.getByRole('button',{name:'Drinks',exact:true}).click();await page.locator('#cards button').click();await page.locator('#running').waitFor();await page.locator('#stop').click();await page.waitForTimeout(600);assert(status.state==='stopping'&&await page.getByRole('button',{name:'Drinks',exact:true}).isDisabled());status={...status,state:'aborted',busy:false};await page.locator('#guest').waitFor();
    for(const width of [1024,736,360,320]){await page.setViewportSize({width,height:1000});for(const target of ['guest','bar','tech']){await page.locator(`[data-view=${target}]`).click();await page.locator('#'+target).waitFor();assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`Overflow ${width} ${target}`);}}
    assert.deepEqual(errors,[]);console.log('Browser login, 8 slots, calibration, busy lock, heartbeat, confirmed stop and 4 widths: PASS');
  }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1});
