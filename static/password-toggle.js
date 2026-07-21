(function () {
    const style = document.createElement("style");
    style.textContent = `
        .tp-password-wrap { position: relative; display: inline-flex; align-items: center; width: 100%; }
        .tp-password-wrap > input { width: 100%; padding-right: 42px !important; }
        .tp-password-toggle { position: absolute; right: 6px; top: 50%; transform: translateY(-50%); width: 32px !important; height: 32px; padding: 0 !important; margin: 0 !important; border: 0; border-radius: 6px; background: transparent !important; color: inherit; cursor: pointer; display: grid; place-items: center; font-size: 18px; line-height: 1; }
        .tp-password-toggle:hover { background: rgba(148, 163, 184, .18) !important; }
    `;
    document.head.appendChild(style);

    function enhance(input) {
        if (!(input instanceof HTMLInputElement) || input.type !== "password" || input.dataset.eyeReady) return;
        input.dataset.eyeReady = "true";
        const wrap = document.createElement("span");
        wrap.className = "tp-password-wrap";
        input.parentNode.insertBefore(wrap, input);
        wrap.appendChild(input);

        const button = document.createElement("button");
        button.type = "button";
        button.className = "tp-password-toggle";
        button.textContent = "👁";
        button.setAttribute("aria-label", "Hiện mật khẩu");
        button.setAttribute("aria-pressed", "false");
        button.addEventListener("click", () => {
            const showing = input.type === "text";
            input.type = showing ? "password" : "text";
            button.textContent = showing ? "👁" : "🙈";
            button.setAttribute("aria-label", showing ? "Hiện mật khẩu" : "Ẩn mật khẩu");
            button.setAttribute("aria-pressed", String(!showing));
            input.focus();
        });
        wrap.appendChild(button);
    }

    function scan(root) {
        if (root.matches && root.matches('input[type="password"]')) enhance(root);
        if (root.querySelectorAll) root.querySelectorAll('input[type="password"]').forEach(enhance);
    }

    function init() {
        scan(document);
        new MutationObserver(records => records.forEach(record => record.addedNodes.forEach(scan)))
            .observe(document.body, { childList: true, subtree: true });
    }

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
    else init();
})();
