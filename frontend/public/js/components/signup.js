export function createSignupPage(root) {
    const div = document.createElement("div");

    const form = document.createElement("form");
    form.className = "form-main";

    // Input email
    const emailElement = document.createElement("label");
    emailElement.textContent = "E-Mail: ";
    emailElement.htmlFor = "email";
    const emailInput = document.createElement("input");
    emailInput.id = "email";
    emailInput.type = "email";
    emailInput.placeholder = "Enter Your Email";
    const emailGroup = document.createElement("div");
    emailGroup.appendChild(emailElement);
    emailGroup.appendChild(emailInput);
    emailGroup.className = "form-group";
    form.appendChild(emailGroup);

    // Input password
    const passElement = document.createElement("label");
    passElement.textContent = "Password: ";
    passElement.htmlFor = "pass";
    const passInput = document.createElement("input");
    passInput.id = "pass"
    passInput.type = "password";
    passInput.placeholder = "Enter your Password";
    const passGroup = document.createElement("div");
    passGroup.appendChild(passElement);
    passGroup.appendChild(passInput);
    passGroup.className = "form-group";
    form.appendChild(passGroup);

    // Phone Number
    const phoneElement = document.createElement("label");
    phoneElement.textContent = "E-Mail: ";
    phoneElement.htmlFor = "phone";
    const phoneInput = document.createElement("input");
    phoneInput.id = "phone";
    phoneInput.type = "tel";
    phoneInput.placeholder = "123 456 7890";
    const phoneGroup = document.createElement("div");
    phoneGroup.appendChild(phoneElement);
    phoneGroup.appendChild(phoneInput);
    phoneGroup.className = "form-group";
    form.appendChild(phoneGroup)

    // Login Button
    const loginButton = document.createElement("button");
    loginButton.type = "submit";
    loginButton.textContent = "Login";
    form.appendChild(loginButton);
    
    form.addEventListener("submit", async (event) => {
        event.preventDefault();

        const email = emailInput.value;
        const pass = passInput.value;

        const data = await login(
            emailInput.value,
            passwordInput.value
        );

        console.log(data);
    })

    div.appendChild(form);
    root.appendChild(div);
}