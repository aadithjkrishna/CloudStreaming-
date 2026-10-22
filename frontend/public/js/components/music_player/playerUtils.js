export const DEFAULT_TITLE = "Nothing Playing";
export const DEFAULT_COVER = "/assets/images/spiderman1.jpg";

export function formatTime(seconds) {
    if (!Number.isFinite(seconds) || seconds < 0) {
        return "0:00";
    }

    const minutes = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);

    return `${minutes}:${String(secs).padStart(2, "0")}`;
}

export function createButton(icon, className = "music-control") {
    const button = document.createElement("button");

    button.type = "button";
    button.className = className;
    button.innerHTML = icon;

    return button;
}
