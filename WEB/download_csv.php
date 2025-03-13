<?php
// File: download_csv.php

// Set headers for CSV download
header('Content-Type: text/csv');
header('Content-Disposition: attachment; filename="STMKG_Weather_Data.csv"');

$host = 'localhost';
$dbname = 'awsp4745_cuaca_db';
$user = 'awsp4745_haidar';
$pass = 'bismillahskripsilancar2025';

try {
    $pdo = new PDO("mysql:host=$host;dbname=$dbname;charset=utf8", $user, $pass);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
} catch (PDOException $e) {
    die("Database connection failed: " . $e->getMessage());
}

// Get date range parameters
$startDate = isset($_GET['start']) ? $_GET['start'] . ' 00:00:00' : date('Y-m-d 00:00:00', strtotime('-7 days'));
$endDate = isset($_GET['end']) ? $_GET['end'] . ' 23:59:59' : date('Y-m-d 23:59:59');

try {
    // Create output stream for CSV
    $output = fopen('php://output', 'w');
    
    // Write CSV header
    fputcsv($output, [
        'Waktu', 
        'Suhu (°C)', 
        'Kelembaban (%)', 
        'Kecepatan Angin (m/s)', 
        'Arah Angin (°)', 
        'Tekanan Udara (hPa)', 
        'Radiasi Matahari (W/m²)', 
        'Curah Hujan (mm)', 
        'Water Level (cm)'
    ]);
    
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
    
    // Fetch and write each row
    while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
        fputcsv($output, [
            $row['waktu'],
            $row['suhu'],
            $row['kelembaban'],
            $row['kecepatan_angin'],
            $row['arah_angin'],
            $row['tekanan_udara'],
            $row['radiasi_matahari'],
            $row['curah_hujan'],
            $row['water_level']
        ]);
    }
    
    // Close the output stream
    fclose($output);
    
} catch (PDOException $e) {
    header('Content-Type: text/plain');
    echo "Error: " . $e->getMessage();
}
?>