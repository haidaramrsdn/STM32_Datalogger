<?php
// File: save_weather.php

$host = 'localhost';
$dbname = 'awsp4745_cuaca_db';
$user = 'awsp4745_haidar';
$pass = 'bismillahskripsilancar2025'; // Ganti dengan password yang benar

try {
    $pdo = new PDO("mysql:host=$host;dbname=$dbname;charset=utf8", $user, $pass);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
} catch (PDOException $e) {
    die("Koneksi database gagal: " . $e->getMessage());
}

// Ambil input mentah dari request
$rawInput = file_get_contents('php://input');
// Simpan data mentah ke file debug.txt untuk memeriksa (pastikan server Anda mengizinkan penulisan file)
file_put_contents('debug.txt', "Raw Input: " . $rawInput . "\n", FILE_APPEND);

// Decode data JSON
$data = json_decode($rawInput, true);

if (!empty($data) && is_array($data)) {
    if ($data && isset($data['waktu'])) {
    // Asumsikan $data['waktu'] = "13/03/2025 06.00.55"
    $dt = DateTime::createFromFormat('d/m/Y H.i.s', $data['waktu']);
    if ($dt) {
        // Ubah ke format MySQL
        $data['waktu'] = $dt->format('Y-m-d H:i:s');
    } else {
        // Jika gagal parse, set null atau default
        $data['waktu'] = null;
    }
}
    $sql = "INSERT INTO weather_data 
            (waktu, latitude, longitude, suhu, kelembaban, kecepatan_angin, arah_angin, tekanan_udara, radiasi_matahari, curah_hujan, water_level)
            VALUES (:waktu, :latitude, :longitude, :suhu, :kelembaban, :kecepatan_angin, :arah_angin, :tekanan_udara, :radiasi_matahari, :curah_hujan, :water_level)";
    $stmt = $pdo->prepare($sql);
    $stmt->execute([
        ':waktu' => isset($data['waktu']) ? $data['waktu'] : null,
        ':latitude' => isset($data['latitude']) ? $data['latitude'] : null,
        ':longitude' => isset($data['longitude']) ? $data['longitude'] : null,
        ':suhu' => isset($data['suhu']) ? $data['suhu'] : null,
        ':kelembaban' => isset($data['kelembaban']) ? $data['kelembaban'] : null,
        ':kecepatan_angin' => isset($data['kecepatan_angin']) ? $data['kecepatan_angin'] : null,
        ':arah_angin' => isset($data['arah_angin']) ? $data['arah_angin'] : null,
        ':tekanan_udara' => isset($data['tekanan_udara']) ? $data['tekanan_udara'] : null,
        ':radiasi_matahari' => isset($data['radiasi_matahari']) ? $data['radiasi_matahari'] : null,
        ':curah_hujan' => isset($data['curah_hujan']) ? $data['curah_hujan'] : null,
        ':water_level' => isset($data['water_level']) ? $data['water_level'] : null,
    ]);
    echo json_encode(["status" => "success"]);
} else {
    echo json_encode(["status" => "error", "message" => "Data tidak diterima"]);
}
?>
