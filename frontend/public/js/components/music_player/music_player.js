
import { ICONS } from "./icons.js";
import {
    DEFAULT_TITLE,
    DEFAULT_COVER,
    formatTime,
    createButton
} from "./playerUtils.js";
import { createAudioController } from "./audioController.js";

export function createMusicPlayer({
    onPlay,
    onPause,
    onNext,
    onPrevious
} = {}) {
    const player = document.createElement("div");
    player.className = "music-div";

    const cover = document.createElement("img");
    cover.className = "cover-image";
    cover.alt = "";

    const title = document.createElement("p");
    title.className = "song-title";

    const seeker = document.createElement("div");
    seeker.className = "seeker-container";

    const currentTime = document.createElement("span");
    currentTime.className = "current-time";
    currentTime.textContent = "0:00";

    const progress = document.createElement("input");
    progress.type = "range";
    progress.className = "music-seeker";
    progress.min = "0";
    progress.max = "100";
    progress.step = "0.1";
    progress.value = "0";
    progress.setAttribute("aria-label", "Seek through song");

    const duration = document.createElement("span");
    duration.className = "duration";
    duration.textContent = "0:00";

    seeker.append(currentTime, progress, duration);

    const controls = document.createElement("div");
    controls.className = "buttons-div";

    const previousButton = createButton(ICONS.previous);
    const playButton = createButton(ICONS.play);
    const nextButton = createButton(ICONS.next);

    previousButton.setAttribute("aria-label", "Previous song");
    playButton.setAttribute("aria-label", "Play");
    nextButton.setAttribute("aria-label", "Next song");

    controls.append(previousButton, playButton, nextButton);
    player.append(cover, title, seeker, controls);

    function renderTrack(song) {
        title.textContent = song?.title || DEFAULT_TITLE;

        const coverSrc = song?.cover_path || DEFAULT_COVER;
        cover.src = coverSrc;

        player.style.setProperty(
            "--cover-background",
            `url("${coverSrc}")`
        );

        currentTime.textContent = "0:00";
        duration.textContent = "0:00";
        progress.value = "0";
    }

    function renderPlaybackState(isPlaying) {
        playButton.innerHTML = isPlaying ? ICONS.pause : ICONS.play;
        playButton.setAttribute(
            "aria-label",
            isPlaying ? "Pause" : "Play"
        );
    }

    const controller = createAudioController({
        onTrackChange: renderTrack,
        onPlaybackStateChange: renderPlaybackState,

        onDurationChange(seconds) {
            duration.textContent = formatTime(seconds);
        },

        onTimeUpdate(time, total) {
            currentTime.textContent = formatTime(time);

            if (Number.isFinite(total) && total > 0) {
                progress.value = (time / total) * 100;
            }
        },

        onPlay,
        onPause,
        onNext,
        onPrevious
    });

    playButton.addEventListener("click", () => {
        controller.togglePlayback();
    });

    previousButton.addEventListener("click", () => {
        controller.previousTrack();
    });

    nextButton.addEventListener("click", () => {
        controller.nextTrack();
    });

    progress.addEventListener("input", () => {
        const total = controller.audio.duration;

        if (Number.isFinite(total) && total > 0) {
            controller.seekTo(
                (Number(progress.value) / 100) * total
            );
        }
    });

    player.audio = controller.audio;
    player.controller = controller;

    controller.initialize();

    return player;
}
