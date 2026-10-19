import { getUsers } from "../api.js";
import { createNavBar } from "../components/navbar.js";
import { createLoginPage } from "../components/login.js";
import { createCanvas } from "../components/myCanvas.js";

let isLogged = false;

const root = document.getElementById("root");

createNavBar(root);

function showHome() {
    const homeText = document.createElement("h1");
    homeText.textContent = "Welcome Home";
    root.appendChild(homeText);
}

if (isLogged) {
    createCanvas();
    showHome();
} else {
    const loginPage = createLoginPage((response) => {
        isLogged = true;
        loginPage.remove();
        showHome();
    });
    root.appendChild(loginPage);
}

const canvas = createCanvas();

root.appendChild(canvas);

const users = await getUsers();

console.log(users);