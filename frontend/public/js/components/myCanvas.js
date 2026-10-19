export function createCanvas() {
    const canvas = document.createElement("canvas");

    canvas.width = 800;
    canvas.height = 400;

    const ctx = canvas.getContext("2d");

    ctx.beginPath();
    ctx.moveTo(60, 40);
    ctx.lineTo(180, 90);
    ctx.lineTo(130, 210);
    ctx.lineTo(10, 160);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = "lightgray";
    for (let i = 0; i < 16; i++) {
        ctx.moveTo(i * 20, 0);
        ctx.lineTo(i * 20, 300);
        ctx.moveTo(0, i * 20);
        ctx.lineTo(300, i * 20);
        ctx.stroke();
    }

    return canvas;
}