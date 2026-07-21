CREATE TABLE IF NOT EXISTS products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,

    name TEXT NOT NULL,
    price INTEGER NOT NULL CHECK(price >= 0),
    description TEXT NOT NULL DEFAULT '',

    is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0, 1)),

    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,

    image_url TEXT,
    category TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    total_price INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS order_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_id   INTEGER NOT NULL,
    product_id INTEGER NOT NULL,
    quantity   INTEGER NOT NULL,
    unit_price INTEGER NOT NULL,
    line_total INTEGER NOT NULL,
    created_at INTEGER NOT NULL,

    FOREIGN KEY(order_id) REFERENCES orders(id)
);

-- Inventory is an append-only ledger. Current stock is SUM(quantity_delta).
CREATE TABLE IF NOT EXISTS inventory_items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, unit TEXT NOT NULL, is_active INTEGER NOT NULL DEFAULT 1, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS inventory_lots (id INTEGER PRIMARY KEY AUTOINCREMENT, item_id INTEGER NOT NULL, quantity REAL NOT NULL, unit_cost INTEGER NOT NULL, purchased_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, created_at INTEGER NOT NULL, is_void INTEGER NOT NULL DEFAULT 0, note TEXT NOT NULL DEFAULT '', FOREIGN KEY(item_id) REFERENCES inventory_items(id));
CREATE TABLE IF NOT EXISTS product_inventory_links (id INTEGER PRIMARY KEY AUTOINCREMENT, product_id INTEGER NOT NULL, item_id INTEGER NOT NULL, quantity REAL NOT NULL, is_active INTEGER NOT NULL DEFAULT 1, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, UNIQUE(product_id,item_id));
CREATE TABLE IF NOT EXISTS inventory_movements (id INTEGER PRIMARY KEY AUTOINCREMENT, item_id INTEGER NOT NULL, lot_id INTEGER, quantity_delta REAL NOT NULL, kind TEXT NOT NULL, reference_type TEXT, reference_id INTEGER, note TEXT NOT NULL DEFAULT '', created_at INTEGER NOT NULL);
