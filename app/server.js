const express = require("express");
const { exec } = require("child_process");
const path = require("path");

const app = express();

app.use(express.json());
app.use(express.static(".")); // Serve form.html + frontend files

app.post("/run", (req, res) => {
    const userInput = req.body.data;

    // FULL path to your C program (Windows needs this)
    const programPath = path.join(__dirname, "compiler.exe");

    // Command to run C program with input
    const command = `"${programPath}" "${userInput}"`;

    exec(command, (err, stdout, stderr) => {
        if (err) {
            res.send("Error running C program: " + err.message);
            return;
        }
        res.send(stdout);
    });
});

app.listen(3000, () =>
    console.log("Server running on http://localhost:3000/form.html")
);
