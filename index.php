<?php
error_reporting(E_ALL);
ini_set('display_errors', '1');

// Check if the "temperature_C" and "beatAvg" parameters are present in the POST request
if (isset($_POST['temperature']) && isset($_POST['beatAvg'])) {
    // Get the temperature and beatAvg from the POST data
    $temperature = $_POST['temperature'];
    $beatAvg = $_POST['beatAvg'];

    // Database credentials
    $servername = "localhost";
    $username = "your_username";
    $password = "your_password";
    $dbname = "your_database";

    // Create a connection
    $conn = new mysqli($servername, $username, $password, $dbname);

    // Check connection
    if ($conn->connect_error) {
        die("Connection failed: " . $conn->connect_error);
    }

    // Use prepared statements to prevent SQL injection
    $sql = "INSERT INTO testdata (temperature, beatAvg) VALUES (? , ?)";
    $stmt = $conn->prepare($sql);

    // Bind parameters
    $stmt->bind_param("ss", $temperature, $beatAvg);

    // Execute the statement
    if ($stmt->execute()) {
        echo "Data inserted into the database successfully";
    } else {
        echo "Error inserting data into the database: " . $stmt->error;
    }

    // Close the statement and connection
    $stmt->close();
    $conn->close();
} else {
    echo "Incomplete data received";
}
?>
