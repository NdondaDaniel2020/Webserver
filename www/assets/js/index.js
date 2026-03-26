// <!-- Particles JS -->

const container = document.getElementById('particles');

for (let i = 0; i < 30; i++) {
    const dot = document.createElement('div');
    dot.classList.add('particle');
    dot.style.left = Math.random() * 100 + '%';
    dot.style.top = Math.random() * 100 + '%';
    dot.style.animationDelay = Math.random() * 5 + 's';
    dot.style.animationDuration = (3 + Math.random() * 4) + 's';
    container.appendChild(dot);
}