(function (global) {
  const pad = value => String(value).padStart(2, '0');
  const localDate = value => `${value.getFullYear()}-${pad(value.getMonth() + 1)}-${pad(value.getDate())}`;
  const epoch = (value, end = false) =>
    Math.floor(new Date(value + (end ? 'T23:59:59' : 'T00:00:00')).getTime() / 1000);
  const round = (value, digits = 0) => Number(value.toFixed(digits));
  const percent = (current, previous) =>
    previous ? round((current - previous) * 100 / previous, 1) : current ? 100 : 0;

  function escapeHtml(value) {
    return String(value).replace(/[&<>"']/g, character => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    })[character]);
  }

  function inlineMarkdown(value) {
    return value
      .replace(/`([^`]+)`/g, '<code>$1</code>')
      .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
      .replace(/__([^_]+)__/g, '<strong>$1</strong>')
      .replace(/\*([^*]+)\*/g, '<em>$1</em>')
      .replace(/_([^_]+)_/g, '<em>$1</em>')
      .replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g,
        '<a href="$2" target="_blank" rel="noopener noreferrer">$1</a>');
  }

  function markdownToHtml(markdown) {
    const source = escapeHtml(markdown).replace(/\r\n?/g, '\n');
    const codeBlocks = [];
    const text = source.replace(/```([^\n]*)\n([\s\S]*?)```/g, (_, language, code) => {
      const token = `@@CODEBLOCK${codeBlocks.length}@@`;
      codeBlocks.push(`<pre><code data-language="${language.trim()}">${code.replace(/^\n|\n$/g, '')}</code></pre>`);
      return token;
    });
    const lines = text.split('\n'), output = [];
    let list = null;
    const closeList = () => { if (list) { output.push(`</${list}>`); list = null; } };
    for (let index = 0; index < lines.length; index++) {
      const line = lines[index];
      if (/^@@CODEBLOCK\d+@@$/.test(line)) { closeList(); output.push(line); continue; }
      if (/^\|.*\|$/.test(line) && index + 1 < lines.length &&
          /^\|?\s*:?-{3,}/.test(lines[index + 1])) {
        closeList();
        const headers = line.split('|').slice(1, -1).map(cell => cell.trim());
        index += 2;
        const rows = [];
        while (index < lines.length && /^\|.*\|$/.test(lines[index])) {
          rows.push(lines[index].split('|').slice(1, -1).map(cell => cell.trim()));
          index++;
        }
        index--;
        output.push(`<table><thead><tr>${headers.map(cell => `<th>${inlineMarkdown(cell)}</th>`).join('')}</tr></thead><tbody>${rows.map(row => `<tr>${row.map(cell => `<td>${inlineMarkdown(cell)}</td>`).join('')}</tr>`).join('')}</tbody></table>`);
        continue;
      }
      const heading = line.match(/^(#{1,3})\s+(.+)$/);
      if (heading) { closeList(); output.push(`<h${heading[1].length}>${inlineMarkdown(heading[2])}</h${heading[1].length}>`); continue; }
      const bullet = line.match(/^\s*[-*+]\s+(.+)$/);
      const numbered = line.match(/^\s*\d+[.)]\s+(.+)$/);
      if (bullet || numbered) {
        const type = bullet ? 'ul' : 'ol';
        if (list !== type) { closeList(); list = type; output.push(`<${type}>`); }
        output.push(`<li>${inlineMarkdown((bullet || numbered)[1])}</li>`);
        continue;
      }
      closeList();
      if (!line.trim()) continue;
      output.push(line.startsWith('&gt; ')
        ? `<blockquote>${inlineMarkdown(line.slice(5))}</blockquote>`
        : `<p>${inlineMarkdown(line)}</p>`);
    }
    closeList();
    return output.join('').replace(/@@CODEBLOCK(\d+)@@/g, (_, id) => codeBlocks[Number(id)]);
  }

  async function getJson(url) {
    const response = await fetch(url);
    const body = await response.json().catch(() => null);
    if (!response.ok)
      throw new Error(body?.message || body?.error?.message || body?.error ||
        `Không tải được dữ liệu (${response.status})`);
    return body;
  }

  async function pages(makeUrl, extract = value => value.items || value.data || []) {
    const rows = [], limit = 500;
    for (let offset = 0; ; offset += limit) {
      const page = extract(await getJson(makeUrl(limit, offset)));
      rows.push(...page);
      if (page.length < limit) break;
      if (rows.length >= 20000)
        throw new Error('Dữ liệu quá lớn. Hãy thu hẹp khoảng thời gian.');
    }
    return rows;
  }

  const csvCell = value => {
    const text = String(value ?? '');
    return /[",\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
  };
  const csv = (headers, rows) =>
    [headers, ...rows].map(row => row.map(csvCell).join(',')).join('\n');

  function aggregateOrders(orders) {
    const products = new Map(), days = new Map();
    let revenue = 0, items = 0;
    for (const order of orders) {
      revenue += order.total_price || 0;
      const date = localDate(new Date(order.created_at * 1000));
      const daily = days.get(date) || { orders: 0, revenue: 0 };
      daily.orders++;
      daily.revenue += order.total_price || 0;
      days.set(date, daily);
      for (const line of order.items || []) {
        const row = products.get(line.product_id) ||
          { name: line.product_name || `SP ${line.product_id}`, qty: 0, revenue: 0 };
        row.qty += line.quantity || 0;
        row.revenue += line.line_total || 0;
        items += line.quantity || 0;
        products.set(line.product_id, row);
      }
    }
    return {
      orders: orders.length, revenue, items,
      aov: orders.length ? round(revenue / orders.length) : 0,
      products, days
    };
  }

  async function compactContext(options) {
    const {
      from, to, orders = true, products = true, inventory = true, compare = true
    } = options;
    const fromEpoch = epoch(from), toEpoch = epoch(to, true);
    const periodDays = Math.round((toEpoch - fromEpoch + 1) / 86400);
    if (!Number.isFinite(periodDays) || periodDays < 1)
      throw new Error('Khoảng thời gian không hợp lệ.');

    const previousTo = fromEpoch - 1;
    const previousFromEpoch = previousTo - periodDays * 86400 + 1;
    const tasks = {};
    if (orders) {
      tasks.currentOrders = pages((limit, offset) =>
        `/api/orders?from=${fromEpoch}&to=${toEpoch}&include_items=true&limit=${limit}&offset=${offset}`);
      tasks.previousOrders = compare ? pages((limit, offset) =>
        `/api/orders?from=${previousFromEpoch}&to=${previousTo}&include_items=true&limit=${limit}&offset=${offset}`) : Promise.resolve([]);
    }
    if (products || orders)
      tasks.products = pages((limit, offset) =>
        `/api/products?active_only=false&limit=${limit}&offset=${offset}`, value => value.data || []);
    if (inventory) {
      tasks.items = pages((limit, offset) => `/api/inventory/items?limit=${limit}&offset=${offset}`);
      tasks.lots = pages((limit, offset) => `/api/inventory/lots?limit=${limit}&offset=${offset}`);
      tasks.movements = pages((limit, offset) =>
        `/api/inventory/movements?from=${fromEpoch}&to=${toEpoch}&limit=${limit}&offset=${offset}`);
    }
    const keys = Object.keys(tasks);
    const values = await Promise.all(Object.values(tasks));
    const data = Object.fromEntries(keys.map((key, index) => [key, values[index]]));
    const sections = [
      `PERIOD,current=${from}..${to}${compare && orders ? `,previous=${localDate(new Date(previousFromEpoch * 1000))}..${localDate(new Date(previousTo * 1000))}` : ''},currency=VND`
    ];

    if (orders) {
      const current = aggregateOrders(data.currentOrders);
      const previous = aggregateOrders(data.previousOrders);
      const summary = [['current', current.orders, current.revenue, current.aov, current.items]];
      if (compare) {
        summary.push(['previous', previous.orders, previous.revenue, previous.aov, previous.items]);
        summary.push(['change_pct', percent(current.orders, previous.orders),
          percent(current.revenue, previous.revenue), percent(current.aov, previous.aov),
          percent(current.items, previous.items)]);
      }
      sections.push('[SUMMARY]\n' +
        csv(['period', 'orders', 'revenue', 'avg_order', 'items'], summary));
      sections.push('[DAILY]\n' + csv(['date', 'orders', 'revenue'],
        [...current.days].sort().map(([date, row]) => [date, row.orders, row.revenue])));
      const productRows = data.products.map(product => {
        const currentRow = current.products.get(product.id) || { qty: 0, revenue: 0 };
        const previousRow = previous.products.get(product.id) || { qty: 0 };
        return [product.name, product.category, currentRow.qty, currentRow.revenue,
          current.revenue ? round(currentRow.revenue * 100 / current.revenue, 1) : 0,
          previousRow.qty, compare ? percent(currentRow.qty, previousRow.qty) : '',
          product.is_active ? 'yes' : 'no'];
      }).sort((a, b) => b[3] - a[3]);
      sections.push('[PRODUCT]\n' + csv(
        ['name', 'category', 'qty', 'revenue', 'revenue_share_pct', 'prev_qty', 'qty_change_pct', 'active'],
        productRows));
    } else if (products) {
      sections.push('[PRODUCT]\n' + csv(['name', 'category', 'price', 'active'],
        data.products.map(product => [product.name, product.category, product.price,
          product.is_active ? 'yes' : 'no'])));
    }

    if (inventory) {
      const usage = new Map();
      for (const movement of data.movements)
        if (movement.quantity_delta < 0)
          usage.set(movement.item_id, (usage.get(movement.item_id) || 0) - movement.quantity_delta);
      const nearestExpiry = new Map(), expiring = new Map();
      for (const lot of data.lots) {
        if (lot.is_void || lot.remaining <= 0) continue;
        const expiryDays = Math.ceil((lot.expires_at - toEpoch) / 86400);
        nearestExpiry.set(lot.item_id,
          Math.min(nearestExpiry.get(lot.item_id) ?? Infinity, expiryDays));
        if (expiryDays <= 7)
          expiring.set(lot.item_id, (expiring.get(lot.item_id) || 0) + lot.remaining);
      }
      const inventoryRows = data.items.map(item => {
        const used = usage.get(item.id) || 0, daily = used / periodDays;
        return [item.name, item.unit, round(item.on_hand, 1), round(used, 1),
          daily > 0 ? round(item.on_hand / daily, 1) : 999,
          nearestExpiry.get(item.id) ?? '', round(expiring.get(item.id) || 0, 1)];
      }).sort((a, b) => a[4] - b[4]);
      sections.push('[INVENTORY]\n' + csv(
        ['name', 'unit', 'on_hand', 'used', 'days_cover', 'expiry_days', 'expiring_7d'],
        inventoryRows));
      const riskyLots = data.lots
        .filter(lot => !lot.is_void && lot.remaining > 0 && lot.expires_at <= toEpoch + 14 * 86400)
        .map(lot => [lot.item_name, round(lot.remaining, 1),
          Math.ceil((lot.expires_at - toEpoch) / 86400)])
        .sort((a, b) => a[2] - b[2]);
      sections.push('[LOT_RISK]\n' +
        csv(['ingredient', 'remaining', 'expires_in_days'], riskyLots));
      sections.push('NOTES: days_cover=on_hand/(used/period_days); 999=no usage. Negative values are urgent risks.');
    }

    const context = sections.join('\n\n');
    if (context.length > 80000)
      throw new Error('Dữ liệu tổng hợp vẫn quá lớn. Hãy thu hẹp khoảng thời gian.');
    return context;
  }

  global.ThimAiData = {
    compactContext, escapeHtml, markdownToHtml, localDate, epoch
  };
})(window);
