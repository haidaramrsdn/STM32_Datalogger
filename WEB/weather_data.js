// Current page for pagination
let currentPage = 1;
const rowsPerPage = 25;
let allData = [];

// Set default date values (last 7 days)
document.addEventListener('DOMContentLoaded', function() {
  const today = new Date();
  const sevenDaysAgo = new Date();
  sevenDaysAgo.setDate(today.getDate() - 7);
  
  document.getElementById('start-date').value = formatDateForInput(sevenDaysAgo);
  document.getElementById('end-date').value = formatDateForInput(today);
  
  // Initial data fetch
  fetchData();
  
  // Event listeners
  document.getElementById('filter-btn').addEventListener('click', fetchData);
  document.getElementById('download-csv').addEventListener('click', downloadCSV);
});

function formatDateForInput(date) {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  return `${year}-${month}-${day}`;
}

function fetchData() {
  const startDate = document.getElementById('start-date').value;
  const endDate = document.getElementById('end-date').value;
  
  if (!startDate || !endDate) {
    alert('Mohon isi tanggal mulai dan tanggal akhir.');
    return;
  }
  
  const tableBody = document.getElementById('table-body');
  tableBody.innerHTML = '<tr><td colspan="9" class="loading">Memuat data...</td></tr>';
  
  // Fetch data from the server
  fetch('get_weather_data.php', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      startDate: startDate,
      endDate: endDate
    })
  })
  .then(response => response.json())
  .then(data => {
    // Store the data for pagination
    allData = data;
    
    // Reset to first page
    currentPage = 1;
    
    // Display the data
    displayData();
  })
  .catch(error => {
    console.error('Error fetching data:', error);
    tableBody.innerHTML = '<tr><td colspan="9" class="no-data">Terjadi kesalahan saat memuat data.</td></tr>';
  });
}

function displayData() {
  const tableBody = document.getElementById('table-body');
  const pagination = document.getElementById('pagination');
  
  // Clear existing content
  tableBody.innerHTML = '';
  pagination.innerHTML = '';
  
  if (allData.length === 0) {
    tableBody.innerHTML = '<tr><td colspan="9" class="no-data">Tidak ada data dalam rentang waktu yang dipilih.</td></tr>';
    return;
  }
  
  // Calculate pagination
  const totalPages = Math.ceil(allData.length / rowsPerPage);
  const start = (currentPage - 1) * rowsPerPage;
  const end = start + rowsPerPage;
  const paginatedData = allData.slice(start, end);
  
  // Display the data for current page
  paginatedData.forEach(item => {
    const row = document.createElement('tr');
    
    // Format the date to be more readable
    const date = new Date(item.waktu);
    const formattedDate = date.toLocaleString('id-ID', {
      day: '2-digit',
      month: '2-digit',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
    
    row.innerHTML = `
      <td>${formattedDate}</td>
      <td>${item.suhu || '-'}</td>
      <td>${item.kelembaban || '-'}</td>
      <td>${item.kecepatan_angin || '-'}</td>
      <td>${item.arah_angin || '-'}</td>
      <td>${item.tekanan_udara || '-'}</td>
      <td>${item.radiasi_matahari || '-'}</td>
      <td>${item.curah_hujan || '-'}</td>
      <td>${item.water_level || '-'}</td>
    `;
    tableBody.appendChild(row);
  });
  
  // Create pagination controls
  if (totalPages > 1) {
    // Previous button
    const prevBtn = document.createElement('button');
    prevBtn.innerHTML = '<i class="fas fa-chevron-left"></i>';
    prevBtn.disabled = currentPage === 1;
    prevBtn.addEventListener('click', () => {
      if (currentPage > 1) {
        currentPage--;
        displayData();
      }
    });
    pagination.appendChild(prevBtn);
    
    // Page numbers
    let startPage = Math.max(1, currentPage - 2);
    let endPage = Math.min(totalPages, startPage + 4);
    
    if (endPage - startPage < 4) {
      startPage = Math.max(1, endPage - 4);
    }
    
    for (let i = startPage; i <= endPage; i++) {
      const pageBtn = document.createElement('button');
      pageBtn.textContent = i;
      pageBtn.classList.toggle('active', i === currentPage);
      pageBtn.style.backgroundColor = i === currentPage ? 'var(--accent-color)' : '';
      pageBtn.addEventListener('click', () => {
        currentPage = i;
        displayData();
      });
      pagination.appendChild(pageBtn);
    }
    
    // Next button
    const nextBtn = document.createElement('button');
    nextBtn.innerHTML = '<i class="fas fa-chevron-right"></i>';
    nextBtn.disabled = currentPage === totalPages;
    nextBtn.addEventListener('click', () => {
      if (currentPage < totalPages) {
        currentPage++;
        displayData();
      }
    });
    pagination.appendChild(nextBtn);
  }
}

function downloadCSV() {
  const startDate = document.getElementById('start-date').value;
  const endDate = document.getElementById('end-date').value;
  
  if (!startDate || !endDate) {
    alert('Mohon isi tanggal mulai dan tanggal akhir.');
    return;
  }
  
  // Check if there's data to download
  if (allData.length === 0) {
    alert('Tidak ada data yang tersedia untuk diunduh.');
    return;
  }
  
  // Create CSV content
  let csvContent = 'Waktu,Suhu (°C),Kelembaban (%),Kecepatan Angin (m/s),Arah Angin (°),Tekanan Udara (hPa),Radiasi Matahari (W/m²),Curah Hujan (mm),Water Level (cm)\n';
  
  allData.forEach(item => {
    const row = [
      item.waktu,
      item.suhu || '',
      item.kelembaban || '',
      item.kecepatan_angin || '',
      item.arah_angin || '',
      item.tekanan_udara || '',
      item.radiasi_matahari || '',
      item.curah_hujan || '',
      item.water_level || ''
    ];
    csvContent += row.join(',') + '\n';
  });
  
  // Create a download link
  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  
  // Format the file name with the date range
  const fileName = `STMKG_Weather_Data_${startDate}_to_${endDate}.csv`;
  
  link.setAttribute('href', url);
  link.setAttribute('download', fileName);
  link.style.visibility = 'hidden';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}