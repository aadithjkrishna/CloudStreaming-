
import { getSongs } from "./songApi.js";

export function createAudioController(callbacks = {}) {
    const audio = new Audio();
    audio.preload = "metadata";

    let songs = [];
    let currentIndex = -1;

    function playTrack(index, autoplay = true) {
        if (!songs.length) return;

        currentIndex = (index + songs.length) % songs.length;

        const song = songs[currentIndex];

        callbacks.onTrackChange?.(song);
        audio.src = song.audio_path;
        audio.load();

        if (autoplay) {
            audio.play().catch(error => {
                console.error("Playback failed:", error);
            });
        }
    }

    function nextTrack() {
        if (currentIndex < 0) return;

        playTrack(currentIndex + 1);
        callbacks.onNext?.();
    }

    function previousTrack() {
        if (currentIndex < 0) return;

        playTrack(currentIndex - 1);
        callbacks.onPrevious?.();
    }

    async function togglePlayback() {
        try {
            if (audio.paused) {
                await audio.play();
            } else {
                audio.pause();
            }
        } catch (error) {
            console.error("Playback failed:", error);
        }
    }

    function seekTo(seconds) {
        if (Number.isFinite(audio.duration) && audio.duration > 0) {
            audio.currentTime = Math.max(
                0,
                Math.min(seconds, audio.duration)
            );
        }
    }

    audio.addEventListener("play", () => {
        callbacks.onPlaybackStateChange?.(true);
        callbacks.onPlay?.();
    });

    audio.addEventListener("pause", () => {
        callbacks.onPlaybackStateChange?.(false);
        callbacks.onPause?.();
    });

    audio.addEventListener("loadedmetadata", () => {
        callbacks.onDurationChange?.(audio.duration);
    });

    audio.addEventListener("timeupdate", () => {
        callbacks.onTimeUpdate?.(
            audio.currentTime,
            audio.duration
        );
    });

    audio.addEventListener("ended", nextTrack);

    async function initialize() {
        try {
            songs = await getSongs();

            if (songs.length) {
                playTrack(0, false);
            } else {
                callbacks.onTrackChange?.(null);
            }
        } catch (error) {
            console.error("Failed to load playlist:", error);
            callbacks.onTrackChange?.(null);
            callbacks.onError?.(error);
        }
    }

    return {
        audio,
        initialize,
        playTrack,
        nextTrack,
        previousTrack,
        togglePlayback,
        seekTo,

        getSongs: () => [...songs],
        getCurrentIndex: () => currentIndex,
        getCurrentSong: () => songs[currentIndex] ?? null
    };
}
