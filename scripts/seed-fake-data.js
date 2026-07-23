#!/usr/bin/env node

const { DatabaseSync } = require('node:sqlite');
const { existsSync, mkdirSync, readdirSync, unlinkSync } = require('node:fs');
const { join, resolve } = require('node:path');

const root = resolve(__dirname, '..');
const dbPath = join(root, 'csdl.sqlite3');
const logsPath = join(root, 'logs');
if (existsSync(dbPath)) unlinkSync(dbPath);
mkdirSync(logsPath, { recursive: true });
for (const entry of readdirSync(logsPath, { withFileTypes: true }))
  if (entry.isFile()) unlinkSync(join(logsPath, entry.name));

const db = new DatabaseSync(dbPath);
db.exec(`
PRAGMA foreign_keys=ON;
CREATE TABLE products(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,price INTEGER NOT NULL CHECK(price>=0),description TEXT NOT NULL DEFAULT '',image_url TEXT DEFAULT NULL,category TEXT NOT NULL DEFAULT '',is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN(0,1)),created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL);
CREATE TABLE orders(id INTEGER PRIMARY KEY AUTOINCREMENT,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,total_price INTEGER NOT NULL);
CREATE TABLE order_items(id INTEGER PRIMARY KEY AUTOINCREMENT,order_id INTEGER NOT NULL,product_id INTEGER NOT NULL,quantity INTEGER NOT NULL,unit_price INTEGER NOT NULL,line_total INTEGER NOT NULL,created_at INTEGER NOT NULL,FOREIGN KEY(order_id) REFERENCES orders(id));
CREATE TABLE inventory_items(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE,unit TEXT NOT NULL,is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN(0,1)),created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL);
CREATE TABLE inventory_lots(id INTEGER PRIMARY KEY AUTOINCREMENT,item_id INTEGER NOT NULL,quantity REAL NOT NULL CHECK(quantity>0),unit_cost INTEGER NOT NULL CHECK(unit_cost>=0),purchased_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,created_at INTEGER NOT NULL,is_void INTEGER NOT NULL DEFAULT 0 CHECK(is_void IN(0,1)),note TEXT NOT NULL DEFAULT '',FOREIGN KEY(item_id) REFERENCES inventory_items(id));
CREATE TABLE product_inventory_links(id INTEGER PRIMARY KEY AUTOINCREMENT,product_id INTEGER NOT NULL,item_id INTEGER NOT NULL,quantity REAL NOT NULL CHECK(quantity>0),is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN(0,1)),created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,UNIQUE(product_id,item_id),FOREIGN KEY(product_id) REFERENCES products(id),FOREIGN KEY(item_id) REFERENCES inventory_items(id));
CREATE TABLE inventory_movements(id INTEGER PRIMARY KEY AUTOINCREMENT,item_id INTEGER NOT NULL,lot_id INTEGER,quantity_delta REAL NOT NULL,kind TEXT NOT NULL,reference_type TEXT,reference_id INTEGER,note TEXT NOT NULL DEFAULT '',created_at INTEGER NOT NULL,FOREIGN KEY(item_id) REFERENCES inventory_items(id),FOREIGN KEY(lot_id) REFERENCES inventory_lots(id));
CREATE TABLE credentials(username TEXT PRIMARY KEY,password TEXT NOT NULL);
CREATE TABLE sessions(token TEXT PRIMARY KEY,username TEXT NOT NULL,expire_at TIMESTAMP NOT NULL);
CREATE TABLE accounts(username TEXT PRIMARY KEY,role TEXT NOT NULL,email TEXT,phone_number TEXT,full_name TEXT,avatar_url TEXT,date_of_birth TEXT,created_at DATETIME DEFAULT CURRENT_TIMESTAMP,updated_at DATETIME DEFAULT CURRENT_TIMESTAMP);
BEGIN IMMEDIATE;
`);

const now = Math.floor(Date.now()/1000), day = 86400;
const products = [
 ['Cà phê sữa đá',32000,'Cà phê',1],['Cà phê đen đá',28000,'Cà phê',1],
 ['Bạc xỉu',36000,'Cà phê',1],['Trà đào cam sả',42000,'Trà',1],
 ['Trà sữa trân châu',45000,'Trà sữa',1],['Matcha latte',48000,'Đá xay',1],
 ['Sinh tố bơ',52000,'Sinh tố',1],['Croissant bơ',30000,'Bánh',1],
 ['Nước ép cam',44000,'Nước ép',1],['Soda việt quất',39000,'Soda',1],
 ['Cold brew cam',49000,'Cà phê',1],['Chocolate nóng',46000,'Đồ nóng',0]
];
const addProduct=db.prepare('INSERT INTO products(name,price,description,category,is_active,created_at,updated_at) VALUES(?,?,?,?,?,?,?)');
products.forEach((p,i)=>addProduct.run(p[0],p[1],`Dữ liệu mẫu: ${p[0]}`,p[2],p[3],now-(120-i)*day,now));

const inventory=[
 ['Hạt cà phê','g',210000,30,90000],['Sữa đặc','ml',62000,180,80000],
 ['Sữa tươi','ml',38000,5,68000],['Trà đen','g',180000,90,18000],
 ['Đào miếng','g',85000,12,22000],['Sả','g',30000,7,4500],
 ['Trân châu','g',55000,20,16000],['Bột matcha','g',420000,120,8000],
 ['Bơ','g',95000,4,12000],['Cam','g',45000,8,42000],
 ['Syrup việt quất','ml',125000,240,26000],['Bánh croissant','cái',18000,3,600],
 ['Đường','g',24000,365,60000],['Đá viên','g',3000,30,600000]
];
const addInventory=db.prepare('INSERT INTO inventory_items(name,unit,created_at,updated_at) VALUES(?,?,?,?)');
inventory.forEach(x=>addInventory.run(x[0],x[1],now-120*day,now));
const recipes={
 1:[[1,18],[2,25],[14,180]],2:[[1,20],[13,12],[14,180]],3:[[1,14],[2,35],[3,60],[14,180]],
 4:[[4,8],[5,45],[6,4],[13,18],[14,180]],5:[[4,7],[3,120],[7,55],[13,18],[14,180]],
 6:[[8,4],[3,160],[13,15],[14,180]],7:[[9,160],[3,80],[2,20],[14,180]],8:[[12,1]],
 9:[[10,240],[13,10],[14,120]],10:[[11,30],[13,15],[14,180]],11:[[1,22],[10,70],[13,12],[14,180]],12:[[3,200],[13,18]]
};
const addRecipe=db.prepare('INSERT INTO product_inventory_links(product_id,item_id,quantity,created_at,updated_at) VALUES(?,?,?,?,?)');
for(const [pid,items] of Object.entries(recipes)) for(const [iid,q] of items) addRecipe.run(+pid,iid,q,now-100*day,now);

const addLot=db.prepare('INSERT INTO inventory_lots(item_id,quantity,unit_cost,purchased_at,expires_at,created_at,note) VALUES(?,?,?,?,?,?,?)');
const addMovement=db.prepare('INSERT INTO inventory_movements(item_id,lot_id,quantity_delta,kind,reference_type,reference_id,note,created_at) VALUES(?,?,?,?,?,?,?,?)');
inventory.forEach((x,i)=>{
 const iid=i+1,bought=now-20*day,expires=(iid===3?now+2*day:iid===12?now+day:bought+x[3]*day);
 const lot=Number(addLot.run(iid,x[4],x[2],bought,expires,bought,iid===11?'Tồn cao do nhập dư':'Lô nhập mẫu').lastInsertRowid);
 addMovement.run(iid,lot,x[4],'purchase','lot',lot,'Nhập kho',bought);
});

let seed=20260723;
const random=()=>((seed=(seed*1664525+1013904223)>>>0)/4294967296);
function pickProduct(age,weekend){
 const weights=[34,19,16,Math.max(5,22-age*.25),13,7,4,weekend?13:7,9,2,Math.min(18,4+age*.22)];
 let cursor=random()*weights.reduce((a,b)=>a+b,0);
 return weights.findIndex(w=>(cursor-=w)<=0)+1;
}
const addOrder=db.prepare('INSERT INTO orders(created_at,updated_at,total_price) VALUES(?,?,?)');
const addOrderItem=db.prepare('INSERT INTO order_items(order_id,product_id,quantity,unit_price,line_total,created_at) VALUES(?,?,?,?,?,?)');
const consume=db.prepare("INSERT INTO inventory_movements(item_id,quantity_delta,kind,reference_type,reference_id,note,created_at) VALUES(?,?,'sale_adjustment','order',?,'Dữ liệu bán hàng mẫu',?)");
for(let age=59;age>=0;age--){
 const date=new Date((now-age*day)*1000),weekend=[0,6].includes(date.getDay());
 const count=Math.round((weekend?27:18)*(1+(59-age)/180)+random()*7);
 for(let n=0;n<count;n++){
  const created=now-age*day-Math.floor(random()*36000),lineCount=random()<.68?1:random()<.9?2:3,lines=[];let total=0;
  for(let line=0;line<lineCount;line++){const pid=pickProduct(age,weekend),q=random()<.86?1:2,price=products[pid-1][1];lines.push([pid,q,price]);total+=q*price}
  const oid=Number(addOrder.run(created,created,total).lastInsertRowid),used=new Map();
  for(const [pid,q,price] of lines){addOrderItem.run(oid,pid,q,price,q*price,created);for(const [iid,amount] of recipes[pid])used.set(iid,(used.get(iid)||0)+amount*q)}
  for(const [iid,amount] of used)consume.run(iid,-amount,oid,created);
 }
}
db.exec(`
INSERT INTO credentials VALUES('admin','mmbbmg'),('manager','demo123');
INSERT INTO accounts(username,role,email,phone_number,full_name) VALUES
('admin','manager','admin@thimpos.local','0900000001','Quản trị viên'),
('manager','manager','manager@thimpos.local','0900000002','Quản lý cửa hàng');
CREATE INDEX idx_orders_created_at ON orders(created_at);
CREATE INDEX idx_order_items_order_id ON order_items(order_id);
CREATE INDEX idx_inventory_movements_created_at ON inventory_movements(created_at);
COMMIT;
`);
const tables=['products','orders','order_items','inventory_items','inventory_lots','inventory_movements'];
const counts=Object.fromEntries(tables.map(t=>[t,db.prepare(`SELECT COUNT(*) count FROM ${t}`).get().count]));
db.close();
console.log(JSON.stringify({database:dbPath,logsCleared:logsPath,counts},null,2));
