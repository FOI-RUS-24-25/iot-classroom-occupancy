const express = require('express');
const sql = require('mssql');
const server = express();
const port = 4000;

const dbConfig = {
    user: 'iotadmin',
    password: 'Rusiot2425',
    server: 'rus-server.database.windows.net',
    database: 'RUSdatabase',
    options: {
        encrypt: true,
        enableArithAbort: true
    }
};

server.use(express.urlencoded({ extended: true }));
server.use(express.json());
server.use(express.static(__dirname));

server.get('/api/last-status', async (req, res) => {
    try {
        await sql.connect(dbConfig);
        const result = await sql.query(`SELECT TOP 1 Status FROM Telemetry ORDER BY ID DESC`);

        console.log("DB Result:", result.recordset); // Log podataka iz baze
        if (result.recordset.length > 0) {
            const status = result.recordset[0].Status;
            console.log("Latest Status Value:", status); // Log zadnjeg statusa
            res.json({ occupied: status });
        } else {
            res.json({ occupied: null });
        }
    } catch (error) {
        console.error("Error fetching last status:", error);
        res.status(500).json({ error: 'Error fetching last status' });
    } finally {
        sql.close();
    }
});

server.get('/api/last-occupied-time', async (req, res) => {
    try {
        await sql.connect(dbConfig);
        const result = await sql.query(`
            SELECT TOP 1 CONVERT(VARCHAR, Timestamp, 126) AS LastOccupiedTimeUTC
            FROM Telemetry
            WHERE Status = 1
            ORDER BY Timestamp DESC
        `);

        res.json({ lastOccupiedTime: result.recordset[0]?.LastOccupiedTimeUTC ?? null });
    } catch (error) {
        console.error("Error fetching last occupied time:", error);
        res.status(500).json({ error: 'Error fetching last occupied time' });
    } finally {
        sql.close();
    }
});

server.get('/api/peak-time', async (req, res) => {
    try {
        await sql.connect(dbConfig);
        const result = await sql.query(`
            SELECT DATEPART(HOUR, Timestamp) AS Hour, COUNT(*) AS Count
            FROM Telemetry 
            WHERE Status = 0
            GROUP BY DATEPART(HOUR, Timestamp)
            ORDER BY Hour ASC
        `);
        res.json(result.recordset);
    } catch (error) {
        res.status(500).json({ error: 'Error fetching peak time' });
    } finally {
        sql.close();
    }
});

server.get('/api/average-occupancy', async (req, res) => {
    try {
        await sql.connect(dbConfig);
        const result = await sql.query(`
            WITH CTE AS (
                SELECT 
                    t1.Timestamp AS StartTime,
                    MIN(t2.Timestamp) AS EndTime
                FROM Telemetry t1
                JOIN Telemetry t2 
                    ON t1.ID < t2.ID 
                    AND t1.Status = 1 
                    AND t2.Status = 0
                    AND t2.Timestamp > t1.Timestamp
                GROUP BY t1.Timestamp
            )
            SELECT AVG(DATEDIFF(SECOND, StartTime, EndTime)) AS AvgTimeSeconds FROM CTE;
        `);
        res.json({ avg_time_seconds: result.recordset[0]?.AvgTimeSeconds ?? "N/A" });
    } catch (error) {
        console.error("Error fetching occupancy duration:", error);
        res.status(500).json({ error: 'Error fetching occupancy duration' });
    } finally {
        sql.close();
    }
});

server.get('/api/daily-summary', async (req, res) => {
    const { date } = req.query;
    if (!date) {
        return res.status(400).json({ error: "Date parameter is required" });
    }

    try {
        await sql.connect(dbConfig);
        
        const result = await sql.query(`
            WITH Occupancy AS (
                SELECT 
                    Timestamp,
                    Status,
                    LEAD(Timestamp) OVER (ORDER BY Timestamp) AS NextTimestamp
                FROM Telemetry
                WHERE CAST(Timestamp AS DATE) = '${date}'
            )
            SELECT 
                SUM(DATEDIFF(MINUTE, Timestamp, NextTimestamp)) AS TotalOccupiedMinutes
            FROM Occupancy
            WHERE Status = 0
        `);

        res.json({ totalOccupiedMinutes: result.recordset[0]?.TotalOccupiedMinutes ?? 0 });
    } catch (error) {
        console.error("Error fetching daily summary:", error);
        res.status(500).json({ error: "Error fetching daily summary" });
    } finally {
        sql.close();
    }
});

server.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
});