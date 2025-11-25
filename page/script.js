// ============================================
// Dashboard Arduino HC-05 - JavaScript
// ============================================

// Estado global de la aplicación
const state = {
    connected: false,
    port: null,
    reader: null,
    messageCount: 0,
    logLines: 0,
    logSize: 0,
    startTime: null,
    uptimeInterval: null
};

// Elementos del DOM
const elements = {
    // Status
    statusText: document.getElementById('statusText'),
    indicatorDot: document.getElementById('indicatorDot'),
    connectionType: document.getElementById('connectionType'),
    connectionAddress: document.getElementById('connectionAddress'),
    
    // Log
    logContainer: document.getElementById('logContainer'),
    logLines: document.getElementById('logLines'),
    logSize: document.getElementById('logSize'),
    
    // Buttons
    connectBtn: document.getElementById('connectBtn'),
    clearBtn: document.getElementById('clearBtn'),
    downloadBtn: document.getElementById('downloadBtn'),
    sendBtn: document.getElementById('sendBtn'),
    
    // Inputs
    baudRate: document.getElementById('baudRate'),
    cmdInput: document.getElementById('cmdInput'),
    
    // Toggles
    autoScroll: document.getElementById('autoScroll'),
    showTimestamp: document.getElementById('showTimestamp'),
    showTokens: document.getElementById('showTokens'),
    colorizeLog: document.getElementById('colorizeLog'),
    
    // Stats
    msgCount: document.getElementById('msgCount'),
    uptime: document.getElementById('uptime'),
    
    // Chart
    dataChart: document.getElementById('dataChart')
};

// ============================================
// Inicialización
// ============================================

document.addEventListener('DOMContentLoaded', () => {
    initializeEventListeners();
    initializeChart();
    
    // Verificar soporte de Web Serial API
    if ('serial' in navigator) {
        appendLog('✅ Web Serial API disponible. Presiona "Conectar" para iniciar.\n');
    } else {
        appendLog('❌ Web Serial API no disponible. Usa Chrome, Edge u Opera.\n');
        elements.connectBtn.disabled = true;
    }
});

// ============================================
// Event Listeners
// ============================================

function initializeEventListeners() {
    // Buttons
    elements.connectBtn.addEventListener('click', handleConnect);
    elements.clearBtn.addEventListener('click', clearLog);
    elements.downloadBtn.addEventListener('click', downloadLog);
    elements.sendBtn.addEventListener('click', sendCommand);
    
    // Quick command buttons
    document.querySelectorAll('.btn-quick').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const cmd = e.target.dataset.cmd;
            if (cmd) {
                elements.cmdInput.value = cmd;
                sendCommand();
            }
        });
    });
    
    // Enter key to send command
    elements.cmdInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter' && state.connected) {
            sendCommand();
        }
    });
    
    // Colorize toggle
    elements.colorizeLog.addEventListener('change', (e) => {
        if (e.target.checked) {
            elements.logContainer.classList.add('colorize');
        } else {
            elements.logContainer.classList.remove('colorize');
        }
    });
}

// ============================================
// Connection Management
// ============================================

async function handleConnect() {
    if (state.connected) {
        disconnect();
    } else {
        await connectSerial();
    }
}

// ============================================
// Serial Connection (Web Serial API)
// ============================================

async function connectSerial() {
    if (!('serial' in navigator)) {
        showNotification('Web Serial API no disponible', 'error');
        appendLog('❌ Web Serial API no disponible. Usa Chrome, Edge u Opera.\n');
        return;
    }
    
    updateStatus('Selecciona el puerto...', 'connecting');
    
    try {
        // Solicitar puerto serial
        state.port = await navigator.serial.requestPort();
        
        const baudRate = parseInt(elements.baudRate.value);
        await state.port.open({ baudRate });
        
        updateStatus('Conectado', 'connected');
        updateConnectionInfo('Serial (HC-05)', `${baudRate} baudios`);
        elements.connectBtn.textContent = 'Desconectar';
        elements.sendBtn.disabled = false;
        state.connected = true;
        startUptime();
        
        appendLog('✅ Conectado al puerto serial\n');
        showNotification('Conectado exitosamente', 'success');
        
        // Iniciar lectura de datos
        readSerialData();
        
    } catch (error) {
        console.error('Serial error:', error);
        appendLog(`❌ Error: ${error.message}\n`);
        showNotification('Error al conectar', 'error');
        disconnect();
    }
}

async function readSerialData() {
    try {
        const decoder = new TextDecoderStream();
        const inputDone = state.port.readable.pipeTo(decoder.writable);
        const inputStream = decoder.readable;
        state.reader = inputStream.getReader();
        
        while (true) {
            const { value, done } = await state.reader.read();
            if (done) {
                appendLog('⚠️ Lectura finalizada\n');
                break;
            }
            if (value) {
                handleIncomingData(value);
            }
        }
    } catch (error) {
        console.error('Read error:', error);
        appendLog(`❌ Error de lectura: ${error.message}\n`);
    }
}

// ============================================
// Disconnect
// ============================================

async function disconnect() {
    try {
        if (state.reader) {
            await state.reader.cancel();
            state.reader = null;
        }
        
        if (state.port) {
            await state.port.close();
            state.port = null;
        }
    } catch (error) {
        console.error('Disconnect error:', error);
    }
    
    state.connected = false;
    
    updateStatus('Desconectado', 'disconnected');
    updateConnectionInfo('-', '-');
    elements.connectBtn.textContent = 'Conectar';
    elements.sendBtn.disabled = true;
    stopUptime();
    
    appendLog('🔌 Desconectado\n');
}

// ============================================
// Data Handling
// ============================================

function handleIncomingData(data) {
    state.messageCount++;
    elements.msgCount.textContent = state.messageCount;
    
    // Procesar tokens especiales si está deshabilitado
    let processedData = data;
    if (!elements.showTokens.checked) {
        processedData = processedData.replace(/<EN>/g, '\n').replace(/<BK>/g, '');
    }
    
    appendLog(processedData);
    updateChartData(data);
}

function appendLog(text) {
    const timestamp = elements.showTimestamp.checked 
        ? `[${new Date().toLocaleTimeString()}] ` 
        : '';
    
    // Colorear según el contenido si está habilitado
    let logEntry = timestamp + text;
    
    if (elements.colorizeLog.checked) {
        if (text.includes('ERROR') || text.includes('❌')) {
            logEntry = `<span class="log-error">${logEntry}</span>`;
        } else if (text.includes('WARNING') || text.includes('⚠️')) {
            logEntry = `<span class="log-warning">${logEntry}</span>`;
        } else if (text.includes('SUCCESS') || text.includes('✅')) {
            logEntry = `<span class="log-success">${logEntry}</span>`;
        } else if (text.includes('INFO') || text.includes('ℹ️') || text.includes('💡')) {
            logEntry = `<span class="log-info">${logEntry}</span>`;
        }
        
        elements.logContainer.innerHTML += logEntry;
    } else {
        elements.logContainer.textContent += logEntry;
    }
    
    // Update stats
    state.logLines++;
    state.logSize = new Blob([elements.logContainer.textContent]).size;
    elements.logLines.textContent = `${state.logLines} líneas`;
    elements.logSize.textContent = `${(state.logSize / 1024).toFixed(2)} KB`;
    
    // Auto-scroll
    if (elements.autoScroll.checked) {
        elements.logContainer.scrollTop = elements.logContainer.scrollHeight;
    }
}

function clearLog() {
    elements.logContainer.textContent = '';
    elements.logContainer.innerHTML = '';
    state.logLines = 0;
    state.logSize = 0;
    elements.logLines.textContent = '0 líneas';
    elements.logSize.textContent = '0 KB';
    appendLog('🗑️ Log limpiado\n');
}

function downloadLog() {
    const content = elements.logContainer.textContent;
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `arduino-log-${new Date().toISOString().slice(0, 10)}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    showNotification('Log descargado', 'success');
}

// ============================================
// Send Commands
// ============================================

async function sendCommand() {
    const cmd = elements.cmdInput.value.trim();
    
    if (!cmd) {
        showNotification('Por favor, ingresa un comando', 'warning');
        return;
    }
    
    if (!state.connected || !state.port) {
        showNotification('No hay conexión activa', 'error');
        return;
    }
    
    try {
        const encoder = new TextEncoder();
        const writer = state.port.writable.getWriter();
        await writer.write(encoder.encode(cmd + '\n'));
        writer.releaseLock();
        
        appendLog(`📤 [CMD] ${cmd}\n`);
        showNotification('Comando enviado', 'success');
        elements.cmdInput.value = '';
    } catch (error) {
        console.error('Send error:', error);
        appendLog(`❌ Error al enviar comando: ${error.message}\n`);
        showNotification('Error al enviar comando', 'error');
    }
}

// ============================================
// UI Updates
// ============================================

function updateStatus(text, status) {
    elements.statusText.textContent = text;
    
    if (status === 'connected') {
        elements.indicatorDot.classList.add('connected');
    } else {
        elements.indicatorDot.classList.remove('connected');
    }
}

function updateConnectionInfo(type, address) {
    elements.connectionType.textContent = type;
    elements.connectionAddress.textContent = address;
}

function showNotification(message, type = 'info') {
    // Simple console notification (you can implement a toast notification system)
    const emoji = {
        success: '✅',
        error: '❌',
        warning: '⚠️',
        info: 'ℹ️'
    };
    
    console.log(`${emoji[type]} ${message}`);
}

// ============================================
// Uptime Timer
// ============================================

function startUptime() {
    state.startTime = Date.now();
    state.uptimeInterval = setInterval(updateUptime, 1000);
}

function stopUptime() {
    if (state.uptimeInterval) {
        clearInterval(state.uptimeInterval);
        state.uptimeInterval = null;
    }
    elements.uptime.textContent = '00:00';
}

function updateUptime() {
    if (!state.startTime) return;
    
    const elapsed = Math.floor((Date.now() - state.startTime) / 1000);
    const minutes = Math.floor(elapsed / 60);
    const seconds = elapsed % 60;
    
    elements.uptime.textContent = `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
}

// ============================================
// Chart (Simple Implementation)
// ============================================

let chartData = [];
let chartCtx = null;

function initializeChart() {
    const canvas = elements.dataChart;
    chartCtx = canvas.getContext('2d');
    
    // Set canvas size
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
    
    drawChart();
}

function updateChartData(data) {
    // Extract numeric values from data (simple example)
    const numbers = data.match(/\d+/g);
    if (numbers && numbers.length > 0) {
        const value = parseInt(numbers[0]);
        chartData.push(value);
        
        // Keep only last 50 data points
        if (chartData.length > 50) {
            chartData.shift();
        }
        
        drawChart();
    }
}

function drawChart() {
    if (!chartCtx) return;
    
    const canvas = elements.dataChart;
    const ctx = chartCtx;
    const width = canvas.width;
    const height = canvas.height;
    
    // Clear canvas
    ctx.clearRect(0, 0, width, height);
    
    if (chartData.length === 0) {
        // Draw placeholder
        ctx.fillStyle = 'rgba(255, 255, 255, 0.1)';
        ctx.font = '14px Inter';
        ctx.textAlign = 'center';
        ctx.fillText('Esperando datos...', width / 2, height / 2);
        return;
    }
    
    // Find min/max for scaling
    const max = Math.max(...chartData, 1);
    const min = Math.min(...chartData, 0);
    const range = max - min || 1;
    
    // Draw line chart
    ctx.strokeStyle = '#00d9ff';
    ctx.lineWidth = 2;
    ctx.beginPath();
    
    const stepX = width / (chartData.length - 1 || 1);
    
    chartData.forEach((value, index) => {
        const x = index * stepX;
        const y = height - ((value - min) / range) * height;
        
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    
    ctx.stroke();
    
    // Draw dots
    ctx.fillStyle = '#00ff88';
    chartData.forEach((value, index) => {
        const x = index * stepX;
        const y = height - ((value - min) / range) * height;
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI * 2);
        ctx.fill();
    });
}

// Redraw chart on window resize
window.addEventListener('resize', () => {
    if (elements.dataChart) {
        elements.dataChart.width = elements.dataChart.offsetWidth;
        elements.dataChart.height = elements.dataChart.offsetHeight;
        drawChart();
    }
});
