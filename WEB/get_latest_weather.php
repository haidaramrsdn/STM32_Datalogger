<?php
// File: get_latest_weather.php

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

$stmt = $pdo->query("SELECT * FROM weather_data ORDER BY created_at DESC LIMIT 1");
$data = $stmt->fetch(PDO::FETCH_ASSOC);
echo json_encode($data);
?>
