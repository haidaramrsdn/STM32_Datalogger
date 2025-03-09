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

// Ganti host dengan URL WebSocket HiveMQ Cloud (port 8884)
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
  
  // Karena kita hanya subscribe ke topik sensor/cuaca, kita parse payload-nya
  if (topic === 'sensor/cuaca') {
    // Payload diharapkan memiliki format:
    // waktu:<value>,latitude:<value>,longitude:<value>,suhu:<value>,kelembaban:<value>,arah_angin:<value>,
    // kecepatan_angin:<value>,tekanan_udara:<value>,radiasi_matahari:<value>,curah_hujan:<value>,water_level:<value>
    const dataPairs = payload.split(',');
    const data = {};
    dataPairs.forEach(pair => {
      const parts = pair.split(':');
      if (parts.length >= 2) {
        const key = parts[0].trim();
        // Jika ada nilai yang mengandung tanda ":" (meski jarang) maka digabung kembali
        const value = parts.slice(1).join(':').trim();
        data[key] = value;
      }
    });
    
    // Update header waktu dan koordinat (gabungkan latitude dan longitude)
    if (data.waktu) {
      document.getElementById('sensor_waktu').textContent = data.waktu;
    }
    if (data.latitude && data.longitude) {
      document.getElementById('sensor_koordinat').textContent = data.latitude + ', ' + data.longitude;
    }
    
    // Daftar parameter sensor yang akan ditampilkan di kartu
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
  }
});
