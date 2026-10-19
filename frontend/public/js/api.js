export async function getUsers() {
    const response = await fetch("/users");
    return response.json();
}

export async function login(email, password) {
    const response = await fetch("/login", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            email,
            password
        })
    });

    return response.json();
}