const express = require('express');
const Database = require('better-sqlite3');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

const app = express();
const db = new Database('recordings.db');

app.use(cors());
app.use(express.json()); // Đọc dữ liệu định dạng JSON từ ESP32-A1S
app.use(express.static(path.join(__dirname, 'public')));

// ==========================================
// KHOỞI TẠO CÁC BẢNG DATABASE
// ==========================================

// 1. Bảng âm thanh (Giữ nguyên cấu trúc của bạn)
db.exec(`
  CREATE TABLE IF NOT EXISTS recordings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    filename    TEXT NOT NULL,
    device_id   TEXT,
    duration    INTEGER,
    size_kb     INTEGER,
    sample_rate INTEGER,
    created_at  TEXT DEFAULT (datetime('now','localtime'))
  )
`);

// 2. Bảng lưu dữ liệu cảm biến khí tượng từ ESP32-A1S
db.exec(`
  CREATE TABLE IF NOT EXISTS sensor_data (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id   TEXT,
    temperature REAL,
    humidity    REAL,
    rain_status TEXT,
    tilt_x      REAL,
    tilt_y      REAL,
    created_at  TEXT DEFAULT (datetime('now','localtime'))
  )
`);

// 3. Bảng lưu lịch sử chụp ảnh của ESP32-CAM
db.exec(`
  CREATE TABLE IF NOT EXISTS images (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    filename    TEXT NOT NULL,
    device_id   TEXT,
    created_at  TEXT DEFAULT (datetime('now','localtime'))
  )
`);

// ==========================================
// CÁC ĐƯỜNG DẪN NHẬN DỮ LIỆU (POST)
// ==========================================

// [API 1] Nhận file ghi âm (.pcm) từ ESP32-A1S
app.post('/upload', (req, res) => {
  const chunks = [];
  req.on('data', chunk => chunks.push(chunk));
  req.on('end', () => {
    const buf = Buffer.concat(chunks);
    const deviceId   = req.headers['x-device-id']    || 'unknown';
    const sampleRate = parseInt(req.headers['x-sample-rate']) || 16000;
    const filename   = `rec_${Date.now()}.pcm`;
    const filepath   = path.join(__dirname, 'recordings', filename);

    fs.mkdirSync(path.dirname(filepath), { recursive: true });
    fs.writeFileSync(filepath, buf);

    const sizeKb   = Math.round(buf.length / 1024);
    const duration = Math.round(buf.length / 2 / sampleRate);

    const result = db.prepare(`
      INSERT INTO recordings (filename, device_id, duration, size_kb, sample_rate)
      VALUES (?, ?, ?, ?, ?)
    `).run(filename, deviceId, duration, sizeKb, sampleRate);

    console.log(`🎵 Audio mới: ${filename} (${sizeKb} KB)`);
    res.json({ id: result.lastInsertRowid, filename, sizeKb, duration });
  });
});

// [API 2] Nhận ảnh thô (.jpg) từ ESP32-CAM
app.post('/upload-image', (req, res) => {
  const chunks = [];
  req.on('data', chunk => chunks.push(chunk));
  req.on('end', () => {
    const buf = Buffer.concat(chunks);
    const deviceId = req.headers['x-device-id'] || 'esp32-cam';
    const filename = `img_${Date.now()}.jpg`;
    const filepath = path.join(__dirname, 'public', 'images', filename);

    fs.mkdirSync(path.dirname(filepath), { recursive: true });
    fs.writeFileSync(filepath, buf);

    const sizeKb = Math.round(buf.length / 1024);

    const result = db.prepare(`
      INSERT INTO images (filename, device_id)
      VALUES (?, ?)
    `).run(filename, deviceId);

    console.log(`📸 Ảnh mới: ${filename} (${sizeKb} KB)`);
    res.json({ success: true, id: result.lastInsertRowid, filename });
  });
});

// [API 3] Nhận gói tin JSON cảm biến từ ESP32-A1S
app.post('/api/sensors', (req, res) => {
  const { device_id, temperature, humidity, rain_status, tilt_x, tilt_y } = req.body;
  try {
    const result = db.prepare(`
      INSERT INTO sensor_data (device_id, temperature, humidity, rain_status, tilt_x, tilt_y)
      VALUES (?, ?, ?, ?, ?, ?)
    `).run(device_id || 'esp32-a1s', temperature, humidity, rain_status, tilt_x, tilt_y);

    console.log(`🌤️ Cảm biến mới: Temp=${temperature}°C | Rain=${rain_status}`);
    res.json({ success: true, id: result.lastInsertRowid });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// ==========================================
// CÁC API TRẢ DỮ LIỆU ĐỂ LÀM DASHBOARD (GET)
// ==========================================
app.get('/api/recordings', (req, res) => {
  res.json(db.prepare('SELECT * FROM recordings ORDER BY id DESC').all());
});

app.get('/api/sensors', (req, res) => {
  res.json(db.prepare('SELECT * FROM sensor_data ORDER BY id DESC LIMIT 100').all());
});

app.get('/api/images', (req, res) => {
  res.json(db.prepare('SELECT * FROM images ORDER BY id DESC').all());
});

app.delete('/api/recordings/:id', (req, res) => {
  db.prepare('DELETE FROM recordings WHERE id=?').run(req.params.id);
  res.json({ ok: true });
});

app.listen(3000, () => console.log('Server chạy tại: http://localhost:3000'));