(function () {
    const NAV_TREE = [
        {
            name: "Quản lý",
            items: [
                { label: "Sản phẩm", href: "/manager/products.html" },
                { label: "Quản lý kho", href: "/manager/inventory.html" },
                { label: "Báo cáo AI", href: "/manager/report.html" },
                { label: "Chat với AI", href: "/manager/chat.html" },
                { label: "Tài khoản nhân viên", href: "/manager/accounts.html" }
            ]
        },
        {
            name: "Bán hàng",
            items: [
                { label: "Tạo đơn hàng", href: "/" },
                { label: "Quản lý đơn hàng", href: "/staff/order-crud.html" }
            ]
        },
        {
            name: "Tài khoản",
            items: [
                { label: "Đăng nhập", href: "/auth/login.html" },
                { label: "Đăng ký", href: "/auth/signup.html" }
            ]
        }
    ];

    function isMobile() {
        return window.innerWidth <= 768;
    }

    async function checkAuth() {
        try {
            const response = await fetch('/auth/me');
            if (response.ok) return await response.json();
        } catch (e) {
            console.error("Auth check failed", e);
        }
        return { authenticated: false };
    }

    async function handleLogout() {
        try {
            const response = await fetch('/auth/logout', { method: 'POST' });
            const result = await response.json();
            if (result.success) {
                window.location.href = "/auth/login.html";
            }
        } catch (e) {
            console.error("Logout failed", e);
        }
    }

    function createNavbar(authState) {
        const nav = document.createElement("div");
        nav.className = "tp-navbar";

        const left = document.createElement("div");
        left.className = "tp-navbar-left";

        const toggleBtn = document.createElement("button");
        toggleBtn.className = "tp-toggle-btn";
        toggleBtn.innerHTML = "☰";
        toggleBtn.onclick = () => {
            document.body.classList.toggle("tp-sidebar-open");
        };

        const logo = document.createElement("img");
        logo.className = "tp-logo";
        logo.src = "/thimpos.png";

        const title = document.createElement("span");
        title.className = "tp-navbar-title";
        title.textContent = "ThimPOS";

        left.append(toggleBtn, logo, title);

        const right = document.createElement("div");
        right.className = "tp-navbar-right";

        const container = document.createElement("div");
        container.className = "tp-auth-container";

        if (authState && authState.authenticated) {
            const user = document.createElement("span");
            user.className = "tp-user-badge";
            user.innerHTML = `${authState.username}`;

            const logout = document.createElement("button");
            logout.className = "tp-btn tp-btn-danger";
            logout.textContent = "Đăng xuất";
            logout.onclick = handleLogout;

            container.append(user, logout);
        } else {
            const login = document.createElement("a");
            login.className = "tp-btn tp-btn-ghost";
            login.href = "/auth/login.html";
            login.textContent = "Đăng nhập";

            const signup = document.createElement("a");
            signup.className = "tp-btn tp-btn-primary";
            signup.href = "/auth/signup.html";
            signup.textContent = "Đăng ký";

            container.append(login, signup);
        }

        right.appendChild(container);
        nav.append(left, right);
        return nav;
    }

    function createSidebar(authState) {
        const sidebar = document.createElement("div");
        sidebar.className = "tp-sidebar";

        const brand = document.createElement("div");
        brand.className = "tp-brand";
        brand.textContent = "ThimPOS";

        sidebar.appendChild(brand);

        NAV_TREE.forEach(folder => {
            const box = document.createElement("div");
            box.className = "tp-folder";

            const title = document.createElement("div");
            title.className = "tp-folder-title";
            title.textContent = folder.name;

            box.appendChild(title);

            folder.items.forEach(item => {
                const a = document.createElement("a");
                a.className = "tp-link";
                a.href = item.href;
                a.textContent = item.label;

                if (window.location.pathname === item.href) {
                    a.classList.add("active");
                }
                
                // Close sidebar when clicking a link on mobile
                a.onclick = () => {
                    if (isMobile()) {
                        document.body.classList.remove("tp-sidebar-open");
                    }
                };

                box.appendChild(a);
            });

            sidebar.appendChild(box);
        });

        return sidebar;
    }

    async function injectLayout() {
        const authState = await checkAuth();
        const body = document.body;

        const navbar = createNavbar(authState);
        const sidebar = createSidebar(authState);

        const main = document.createElement("div");
        main.className = "tp-main";

        // Di chuyển content hiện tại vào main
        while (body.firstChild) {
            main.appendChild(body.firstChild);
        }

        const overlay = document.createElement("div");
        overlay.className = "tp-overlay";
        overlay.onclick = () => {
            document.body.classList.remove("tp-sidebar-open");
        };

        body.append(navbar, sidebar, overlay, main);

        // Khởi tạo trạng thái mặc định: Mobile đóng, Desktop mở
        if (isMobile()) {
            body.classList.remove("tp-sidebar-open");
        } else {
            body.classList.add("tp-sidebar-open");
        }

        window.addEventListener("resize", () => {
            if (isMobile()) {
                body.classList.remove("tp-sidebar-open");
            } else {
                // Tự động mở lại khi quay về màn hình desktop (tùy chọn UI)
                body.classList.add("tp-sidebar-open");
            }
        });
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", injectLayout);
    } else {
        injectLayout();
    }
})();
