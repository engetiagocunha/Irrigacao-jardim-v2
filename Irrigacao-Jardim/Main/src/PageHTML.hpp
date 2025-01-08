#ifndef PAGEHTML_HPP
#define PAGEHTML_HPP

// Html da página de login
const char LOGIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #DCDCDC; }
.login-container { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%);
                   width: 300px; padding: 20px; background-color: #C0C0C0; border-radius: 10px;  box-shadow: 0px 2px 2px 0px rgba(0, 0, 0, 0.2); }
input[type=text], input[type=password] { width: 100%; padding: 12px 20px; margin: 8px 0; display: inline-block;
                                         border: 1px solid #C0C0C0; border-radius: 10px; box-sizing: border-box; background: #DCDCDC; }
input[type=submit] { width: 100%; background-color: #4CAF50; color: white; padding: 14px 20px; margin: 8px 0;
                    border: none; border-radius: 10px; cursor: pointer; }
input[type=submit]:hover { background-color: #45a049; }
</style>
</head>
<body>
<div class="login-container">
<h2>Login</h2>
<form action="/login" method="POST">
<input type="text" name="username" placeholder="Usuário">
<br>
<input type="password" name="password" placeholder="Senha">
<br>
<input type="submit" value="Entrar">
</form>
</div>
</body>
</html>
)rawliteral";


// Html da tela principal
const char HOME_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { 
  font-family: Arial, sans-serif; 
  text-align: center; 
  margin: 0; 
  padding: 0; 
  background-color: #A9A9A9;
}

.header-container { 
  display: flex; 
  align-items: center; 
  justify-content: space-between; 
  padding: 10px 20px;
  position: relative;
}

.header-left { 
  font-weight: bold;
  font-size: 24px; 
  color: #333; 
}

.header-right button { 
  background-color: #D3D3D3; 
  border: none; 
  border-radius: 10px; 
  padding: 10px; 
  width: 80px; 
  font-size: 18px;
  cursor: pointer; 
  box-shadow: 0px 2px 2px 0px rgba(0, 0, 0, 0.2);
}

.temp-sensor { 
 display: flex; 
  justify-content: center; 
  padding: 10px; 
  margin-bottom: 5px; 
}

.temp-sensor-item { 
  background-color: #D3D3D3;
  padding: 5px;
  margin: 10px;
  flex: 1;
  max-width: 50%;
  text-align: center;
  border-radius: 10px;
  box-shadow: 0px 2px 2px 0px rgba(0, 0, 0, 0.2);
}

.soil-sensor {
  display: flex; 
  justify-content: center; 
  margin-bottom: 5px;
}

.soil-sensor-item {
  background-color: #D3D3D3;
  flex: 1;
  text-align: center;
  border-radius: 10px;
  margin-left: 20px;
  margin-right: 20px;
  box-shadow: 0px 2px 2px 0px rgba(0, 0, 0, 0.2);
}

.grid { 
  display: flex; 
  justify-content: center;
  margin: 20px;
}

.grid-item { 
  display: flex; 
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: 100%; 
}

.switch {
  position: relative;
  display: inline-block;
  width: 120px;
  height: 60px;
  transform: rotate(-90deg);
}
.switch input { 
  opacity: 0;
  width: 0;
  height: 0;
}
.slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #808080;
  transition: .4s;
  border-radius: 10px;
}
.slider:before {
  position: absolute;
  content: "";
  height: 52px;
  width: 52px;
  left: 4px;
  bottom: 4px;
  background-color: white;
  transition: .4s;
  border-radius: 10px;
}
input:checked + .slider {
 background-color: #f9d835;
 box-shadow: 0 0 10px #f9d835;
}
input:checked + .slider:before {
  transform: translateX(60px);
}

/* Mensagem de erro de conexão */
#connection-status {
  color: red;
  font-weight: bold;
  display: none;
}

</style>
</head>
<body>

<div class="header-container">
  <div class="header-left">ESP SH v1</div>
  <div class="header-right">
    <button onclick="location.href='/logout'">
      Sair
    </button>
  </div>
</div>

<p id="connection-status">❌ Conexão perdida com o servidor!</p>

<div class="temp-sensor">
  <div class="temp-sensor-item">
    <p>🌡️Temperatura</p>  
    <p><span id="temperature">%TEMPERATURE%</span> °C</p>
  </div>
  <div class="temp-sensor-item">
    <p>💧Umidade</p>
    <p><span id="humidity">%HUMIDITY%</span> %</p>
  </div>
</div>

<div class="soil-sensor">
  <div class="soil-sensor-item">
    <p>🌱Umidade do Solo</p>
    <p><span id="soilMoisture">%SOIL_MOISTURE%</span> %</p>
  </div>
</div>

<div class="grid">
  <!-- Único botão para controle do dispositivo -->
  <div class="grid-item">
    <p>Dispositivo</p>
    <label class="switch">
      <input type="checkbox" id="toggleBtn" onchange="toggleDevice()">
      <span class="slider"></span>
    </label>
  </div>
</div>

<script>
let ws = new WebSocket('ws://' + window.location.hostname + ':81');
let isConnected = false;

ws.onopen = function() {
  isConnected = true;
  updateConnectionStatus();
};

ws.onclose = function() {
  isConnected = false;
  updateConnectionStatus();
};

ws.onerror = function() {
  isConnected = false;
  updateConnectionStatus();
};

ws.onmessage = function(event) {
  let data = JSON.parse(event.data);
  
  // Atualiza o estado do botão de controle
  let btn = document.getElementById('toggleBtn');
  if (btn) {
    btn.checked = data.deviceState; // Atualiza o estado do botão
    btn.disabled = !isConnected;
  }

  document.getElementById('temperature').textContent = data.temperature.toFixed(1);
  document.getElementById('humidity').textContent = data.humidity.toFixed(1);
  if (document.getElementById('soilMoisture')) {
    document.getElementById('soilMoisture').textContent = data.soilMoisture.toFixed(1);
  }
};

function updateConnectionStatus() {
  const statusMessage = document.getElementById('connection-status');
  if (isConnected) {
    statusMessage.style.display = 'none';
  } else {
    statusMessage.style.display = 'block';
  }
}

function toggleDevice() {
  if (isConnected) {
    ws.send('toggle'); // Envia o comando para o WebSocket
  } else {
    alert('Conexão perdida! Tente novamente quando a conexão for restabelecida.');
  }
}
</script>
</body>
</html>

)rawliteral";


#endif  // PAGEHTML_HPP