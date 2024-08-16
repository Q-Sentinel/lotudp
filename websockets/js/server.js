const ws = new WebSocket('ws://localhost:9002');

// Send subscription request
ws.onopen = () => {
    ws.send(JSON.stringify({ subscribe: 'topic1' }));
};

// Handle incoming messages
ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    console.log('Received update:', message);
    console.log('Received update:', message.message);

    const updatesDiv = document.getElementById('updates');
    updatesDiv.textContent = `Received update: ${message.message}`;
};

// To update the topic
function updateTopic(topic) {
    ws.send(JSON.stringify({ update: topic }));
}