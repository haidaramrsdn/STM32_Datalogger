// ========= KONFIGURASI KONEKSI MQTT via WEBSOCKET =========
const options = {
  connectTimeout: 4000,
  clientId: 'webClient-' + Math.random().toString(16).substr(2, 8),
  // Sesuaikan username & password HiveMQ Cloud Anda
  username: 'haidaramrurusdan',
  password: 'h18082746R',
  keepalive: 60,
  clean: true,
};

const host = 'wss://a51ad753198b41b3a4c2f4488d3e409d.s1.eu.hivemq.cloud:8884/mqtt';
const client = mqtt.connect(host, options);

// Definisikan unit untuk setiap sensor
const units = {
  suhu: '°C',
  kelembaban: '%',
  kecepatan_angin: 'm/s',
  arah_angin: '°',
  tekanan_udara: 'hPa',
  curah_hujan: 'mm',
  water_level: 'cm',
  radiasi_matahari: 'W/m²'
};

client.on('connect', () => {
  const statusText = document.getElementById('mqtt_status');
  const indicator = document.getElementById('connection-indicator');
  
  statusText.textContent = 'Terhubung';
  indicator.classList.add('connected');
  
  console.log('Terhubung ke MQTT broker via WebSocket.');
  
  // Subscribe ke topik sensor/cuaca
  client.subscribe('sensor/cuaca', (err) => {
    if (err) {
      console.error('Gagal subscribe:', err);
    } else {
      console.log('Berhasil subscribe ke topik: sensor/cuaca');
    }
  });
});

client.on('error', (err) => {
  const statusText = document.getElementById('mqtt_status');
  const indicator = document.getElementById('connection-indicator');
  
  statusText.textContent = 'Error';
  indicator.classList.remove('connected');
  
  console.error('MQTT Error:', err);
});

client.on('message', (topic, message) => {
  const payload = message.toString();
  console.log(`Topik: ${topic}, Pesan: ${payload}`);
  
  if (topic === 'sensor/cuaca') {
    // Parse payload
    const dataPairs = payload.split(',');
    const data = {};
    dataPairs.forEach(pair => {
      const parts = pair.split(':');
      if (parts.length >= 2) {
        const key = parts[0].trim();
        const value = parts.slice(1).join(':').trim();
        data[key] = value;
      }
    });
    
    // Debug: Tampilkan data di console sebelum mengirim
    console.log("Data yang akan dikirim:", data);
    
    // Update waktu dan koordinat
    if (data.waktu) {
      document.getElementById('sensor_waktu').textContent = data.waktu;
    }
    if (data.latitude && data.longitude) {
      document.getElementById('sensor_koordinat').textContent = data.latitude + ', ' + data.longitude;
    }
    
    const sensorKeys = [
      'suhu',
      'kelembaban',
      'arah_angin',
      'kecepatan_angin',
      'tekanan_udara',
      'radiasi_matahari',
      'curah_hujan',
      'water_level'
    ];
    
    sensorKeys.forEach(key => {
      const cardElement = document.getElementById(key);
      if (cardElement && data[key] !== undefined) {
        const valueElement = cardElement.querySelector('.card-value');
        if (valueElement) {
          valueElement.innerHTML = `${data[key]}<span class="card-unit">${units[key] || ''}</span>`;
          cardElement.classList.add('highlight');
          setTimeout(() => cardElement.classList.remove('highlight'), 800);
        }
      }
    });
    
    
    
    // Kirim data ke server untuk disimpan
    fetch('save_weather.php', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(data)
    })
    .then(response => response.json())
    .then(result => console.log('Response dari server:', result))
    .catch(error => console.error('Error saat mengirim data:', error));
  }
});

// Saat halaman dimuat, ambil data terbaru dari server
window.addEventListener('load', () => {
  fetch('get_latest_weather.php')
    .then(response => response.json())
    .then(data => {
      if (data) {
        if (data.waktu) {
          document.getElementById('sensor_waktu').textContent = data.waktu;
        }
        if (data.latitude && data.longitude) {
          document.getElementById('sensor_koordinat').textContent = data.latitude + ', ' + data.longitude;
        }
        
        const sensorKeys = [
          'suhu',
          'kelembaban',
          'arah_angin',
          'kecepatan_angin',
          'tekanan_udara',
          'radiasi_matahari',
          'curah_hujan',
          'water_level'
        ];
        
        sensorKeys.forEach(key => {
          const cardElement = document.getElementById(key);
          if (cardElement && data[key] !== undefined) {
            const valueElement = cardElement.querySelector('.card-value');
            if (valueElement) {
              valueElement.innerHTML = `${data[key]}<span class="card-unit">${units[key] || ''}</span>`;
            }
          }
        });
      }
    })
    .catch(error => console.error('Error fetching latest data:', error));
});
