export async function getSongs() {
    const response = await fetch("/songs");

    if (!response.ok) {
        throw new Error(`Failed to fetch songs: HTTP ${response.status}`);
    }

    const data = await response.json();

    if (!Array.isArray(data.songs)) {
        throw new Error("Invalid songs API response");
    }

    return data.songs;
}
