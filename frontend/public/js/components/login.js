import { login } from "../api.js";

export function createLoginPage(onLoginSuccess) {
    const div = document.createElement("div");
    const form = document.createElement("form");

    const emailElement = document.createElement("label");
    emailElement.textContent = "E-Mail: ";
    emailElement.htmlFor = "email";

    const emailInput = document.createElement("input");
    emailInput.id = "email";
    emailInput.type = "email";
    emailInput.placeholder = "Enter Your Email";

    const passElement = document.createElement("label");
    passElement.textContent = "Password: ";
    passElement.htmlFor = "pass";

    const passInput = document.createElement("input");
    passInput.id = "pass";
    passInput.type = "password";
    passInput.placeholder = "Enter your Password";

    const emailGroup = document.createElement("div");
    emailGroup.appendChild(emailElement);
    emailGroup.appendChild(emailInput);

    const passGroup = document.createElement("div");
    passGroup.appendChild(passElement);
    passGroup.appendChild(passInput);

    const loginButton = document.createElement("button");
    loginButton.type = "submit";
    loginButton.textContent = "Login";

    emailGroup.className = "form-group";
    passGroup.className = "form-group";
    form.className = "form-main";

    form.appendChild(emailGroup);
    form.appendChild(passGroup);
    form.appendChild(loginButton);

    form.addEventListener("submit", async (event) => {
        event.preventDefault();

        const email = emailInput.value;
        const pass = passInput.value;

        const response = await login(email, pass);

        if (response.status === "success") {
            console.log("Login successful");

            onLoginSuccess(response);
        }

        console.log(response);
    });

    div.appendChild(form);
    return div;
}