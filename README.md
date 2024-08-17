

# QSentinel LoutUDP Protocol

## Overview

This project implements a server that uses WebSocket for real-time communication and UDP for packet transmission. The server manages topics and periodically clears data to optimize performance.

### Features

-WebSocket Server: Allows real-time communication with clients via WebSocket.
-UDP Communication: Handles packet reception and transmission using UDP sockets.
-Topic Management: Manages topics, where each topic can hold key-value pairs and be used to send notifications to subscribers.
-Periodic Clearing: Clears all topics periodically to prevent memory buildup.
