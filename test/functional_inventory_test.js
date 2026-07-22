const baseUrl = process.env.THIMPOS_URL || "http://127.0.0.1";

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

async function request(path, { method = "GET", body, cookie, expected } = {}) {
  const response = await fetch(baseUrl + path, {
    method,
    headers: {
      ...(body ? { "content-type": "application/json" } : {}),
      ...(cookie ? { cookie } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await response.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = text; }
  if (expected !== undefined) {
    assert(response.status === expected,
      `${method} ${path}: expected ${expected}, got ${response.status}: ${text}`);
  } else {
    assert(response.ok, `${method} ${path}: HTTP ${response.status}: ${text}`);
  }
  return { response, data };
}

async function login(username, password) {
  const { response, data } = await request("/auth/login", {
    method: "POST", body: { username, password }, expected: 200,
  });
  assert(data.success, `Login failed for ${username}`);
  const setCookie = response.headers.get("set-cookie") || "";
  const sid = setCookie.match(/sid=[^;]+/)?.[0];
  assert(sid, `Login for ${username} did not return sid cookie`);
  return sid;
}

async function main() {
  const stamp = Date.now();
  const managerCookie = await login("admin", "mmbbmg");
  const auth = { cookie: managerCookie };

  const product = (await request("/api/products", {
    ...auth, method: "POST", body: {
      name: `Pizza xúc xích gà sample ${stamp}`,
      price: 79000,
      description: "Dữ liệu functional test",
      image_url: "",
      category: "food",
    }, expected: 201,
  })).data;

  const ingredients = [];
  for (const [name, unit] of [
    [`Đế pizza sample ${stamp}`, "cái"],
    [`Xúc xích sample ${stamp}`, "miếng"],
    [`Gà sample ${stamp}`, "miếng"],
  ]) {
    ingredients.push((await request("/api/inventory/items", {
      ...auth, method: "POST", body: { name, unit }, expected: 201,
    })).data.id);
  }

  const purchasedAt = Math.floor(Date.now() / 1000);
  const expiresAt = purchasedAt + 30 * 86400;
  for (const [index, itemId] of ingredients.entries()) {
    await request("/api/inventory/lots", {
      ...auth, method: "POST", body: {
        item_id: itemId,
        quantity: [20, 10000, 1000000][index],
        unit_cost: [12000, 500, 10][index],
        purchased_at: purchasedAt,
        expires_at: expiresAt,
        note: "Functional test sample lot",
      }, expected: 201,
    });
  }

  for (const [index, itemId] of ingredients.entries()) {
    await request("/api/inventory/recipes", {
      ...auth, method: "POST", body: {
        product_id: product.id,
        item_id: itemId,
        quantity: [1, 100, 10000][index],
      }, expected: 201,
    });
  }

  const before = (await request("/api/inventory/items", auth)).data;
  const order = (await request("/api/orders", {
    ...auth, method: "POST", body: {
      items: [{ product_id: product.id, quantity: 1 }],
    }, expected: 201,
  })).data;
  assert(order.id > 0, "Order was not created");

  await request(`/api/orders/${order.id}`, {
    ...auth, method: "PUT", body: {
      items: [{ product_id: product.id, quantity: 2 }],
    }, expected: 200,
  });
  const after = (await request("/api/inventory/items", auth)).data;
  const expectedUsage = [2, 200, 20000];
  ingredients.forEach((id, index) => {
    const oldStock = before.find((item) => item.id === id).on_hand;
    const newStock = after.find((item) => item.id === id).on_hand;
    assert(oldStock - newStock === expectedUsage[index],
      `Inventory item ${id}: expected usage ${expectedUsage[index]}, got ${oldStock - newStock}`);
  });

  const staffName = `sample_staff_${stamp}`;
  const staffPassword = `Sample-${stamp}`;
  await request("/api/accounts", {
    ...auth, method: "POST", body: {
      username: staffName, role: "staff", email: `${staffName}@example.test`,
      phone_number: "", full_name: "Nhân viên sample", avatar_url: "", date_of_birth: "",
    }, expected: 201,
  });
  await request("/auth/register", {
    method: "POST", body: { username: staffName, password: staffPassword }, expected: 200,
  });
  const staffCookie = await login(staffName, staffPassword);
  await request("/api/inventory/items", { cookie: staffCookie, expected: 403 });
  await request("/api/orders", {
    cookie: staffCookie, method: "POST",
    body: { items: [{ product_id: product.id, quantity: 1 }] }, expected: 201,
  });

  console.log(JSON.stringify({
    success: true,
    sample: { product_id: product.id, order_id: order.id, ingredient_ids: ingredients, staff: staffName },
    checks: ["manager login", "inventory CRUD", "recipe", "order consumption", "order replacement", "staff denied inventory", "staff creates order"],
  }, null, 2));
}

main().catch((error) => {
  console.error(error.stack || error);
  process.exitCode = 1;
});
