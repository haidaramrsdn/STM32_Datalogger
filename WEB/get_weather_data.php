<?php
// File: get_weather_data.php

header('Content-Type: application/json');

$host = 'localhost';
$dbname = 'awsp4745_cuaca_db';
$user = 'awsp4745_haidar';
$pass = 'bismillahskripsilancar2025';

try {
    $pdo = new PDO("mysql:host=$host;dbname=$dbname;charset=utf8", $user, $pass);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
} catch (PDOException $e) {
    echo json_encode(["error" => "Koneksi database gagal: " . $e->getMessage()]);
    exit;
}

// Get JSON input
$input = json_decode(file_get_contents('php://input'), true);

// Validate input
if (!isset($input['startDate']) || !isset($input['endDate'])) {
    echo json_encode(["error" => "Parameter startDate dan endDate dibutuhkan"]);
    exit;
}

// Parse dates
$startDate = $input['startDate'] . ' 00:00:00';
$endDate = $input['endDate'] . ' 23:59:59';

try {
    // Prepare and execute query
    $stmt = $pdo->prepare("
        SELECT 
            waktu, suhu, kelembaban, kecepatan_angin, arah_angin, 
            tekanan_udara, radiasi_matahari, curah_hujan, water_level
        FROM 
            weather_data
        WHERE 
            waktu BETWEEN :startDate AND :endDate
        ORDER BY 
            waktu DESC
    ");
    
    $stmt->execute([
        ':startDate' => $startDate,
        ':endDate' => $endDate
    ]);
    
    // Fetch all results
    $data = $stmt->fetchAll(PDO::FETCH_ASSOC);
    
    // Return the data as JSON
    echo json_encode($data);
    
} catch (PDOException $e) {
    echo json_encode(["error" => "Error querying database: " . $e->getMessage()]);
    exit;
}
?>