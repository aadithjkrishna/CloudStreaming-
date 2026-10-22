import { createNavBar } from "../components/navbar.js";
import { createMusicPlayer } from "../components/music_player/music_player.js";

const root = document.getElementById("root");

// root.appendChild(createNavBar());

const player = createMusicPlayer({
  onPlay: () => console.log("Playing"),
  onPause: () => console.log("Paused"),
  onNext: () => console.log("Skipped to next song"),
  onPrevious: () => console.log("Skipped to previous song")
});

const audio = player.audio;

root.appendChild(player);