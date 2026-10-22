export function createNavBar() {
    const links = ["Home", "Gallery", "About"];

    const nav = document.createElement("nav");
    nav.className = "nav";

    links.forEach((link) => {
        const a = document.createElement("a");

        a.textContent = link;
        a.href = `/${link.toLowerCase()}`;
        a.className = "nav-item";

        nav.appendChild(a);
    });

    // root.appendChild(nav);
    return nav;
}