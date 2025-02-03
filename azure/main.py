import azure.functions as func
import logging
import json
import pyodbc

# Kreiranje aplikacije
app = func.FunctionApp(http_auth_level=func.AuthLevel.FUNCTION)

# Connection string za SQL bazu
conn_str = "Driver={ODBC Driver 17 for SQL Server};Server=tcp:rus-server.database.windows.net,1433;Database=RUSdatabase;Uid=iotadmin;Pwd=Rusiot2425;Encrypt=yes;TrustServerCertificate=no;Connection Timeout=30;"

# Funkcija za unos podataka u SQL bazu
def insert_to_database(device_id, status, timestamp):
    try:
        conn = pyodbc.connect(conn_str)
        cursor = conn.cursor()
        cursor.execute("INSERT INTO Telemetry (DeviceID, Status, Timestamp) VALUES (?, ?, ?)", 
                       (device_id, status, timestamp))
        conn.commit()
        conn.close()
    except Exception as e:
        logging.error(f"Database insertion failed: {str(e)}")
        raise

@app.route(route="send_telemetry")
def send_telemetry(req: func.HttpRequest) -> func.HttpResponse:
    logging.info('Processing a request from ESP32.')

    try:
        # Parse JSON iz zahtjeva
        req_body = req.get_json()
        device_id = req_body.get('DeviceID')
        status = req_body.get('Status')
        timestamp = req_body.get('Timestamp')

        # Provjera da li podaci sadrže potrebne vrijednosti
        if device_id and status is not None and timestamp:
            logging.info(f"Received data: DeviceID={device_id}, Status={status}, Timestamp={timestamp}")
            
            # Poziv funkcije za unos podataka u SQL bazu
            try:
                insert_to_database(device_id, status, timestamp)
                logging.info("Data successfully stored in the database.")
                return func.HttpResponse(
                    json.dumps({"message": "Data received and stored successfully.", "data": req_body}),
                    status_code=200,
                    mimetype="application/json"
                )
            except Exception as e:
                logging.error(f"Failed to store data in the database: {str(e)}")
                return func.HttpResponse(
                    "Failed to store data in the database.",
                    status_code=500
                )
        else:
            logging.warning("Invalid JSON format. Expected fields: DeviceID, Status, Timestamp.")
            return func.HttpResponse(
                "Invalid JSON format. Expected fields: DeviceID, Status, Timestamp.",
                status_code=400
            )
    except ValueError:
        logging.error("Invalid JSON received.")
        return func.HttpResponse(
            "Invalid JSON format.",
            status_code=400
        )
