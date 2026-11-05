const subjectsContainer = document.getElementById("subjects");
const results = document.getElementById("results");

let currentSubjects = [];

// =====================================
// 1. SAFE SERVER RESPONSE HANDLER
// =====================================

async function fetchJSON(url, options = {}) {
    const response = await fetch(url, options);

    const text = await response.text();

    let data;

    try {
        data = JSON.parse(text);
    } catch (error) {
        if (response.status === 403) {
            throw new Error(
                "403 Forbidden: Your C server is blocking this request. Check the API route and server permissions."
            );
        }

        if (response.status === 404) {
            throw new Error("API route not found: " + url);
        }

        if (!response.ok) {
            throw new Error(
                `Server error ${response.status}: ${text.substring(0, 150)}`
            );
        }

        throw new Error(
            "Invalid server response. Expected JSON but received: " +
            text.substring(0, 100)
        );
    }

    if (!response.ok) {
        throw new Error(
            data.error || data.message || `Server error: ${response.status}`
        );
    }

    return data;
}

// =====================================
// 2. ADD SUBJECT
// =====================================

function addSubject() {

    if (subjectsContainer.children.length >= 10) {
        alert("Maximum 10 subjects allowed.");
        return;
    }

    const row = document.createElement("div");
    row.className = "subject-row";

    const fields = [
        ["text", "Subject name", "name"],
        ["number", "Credits", "credits"],
        ["number", "Internal /50", "internal"],
        ["number", "External /100", "external"]
    ];

    fields.forEach(([type, placeholder, name]) => {

        const input = document.createElement("input");

        input.type = type;
        input.placeholder = placeholder;
        input.className = name;

        if (type === "number") {
            input.min = name === "credits" ? "1" : "0";

            input.max = name === "internal" ? "50" : "100";

            input.step = name === "credits" ? "1" : "any";
        }

        row.appendChild(input);
    });

    const remove = document.createElement("button");

    remove.className = "remove";
    remove.textContent = "X";
    remove.type = "button";

    remove.onclick = () => row.remove();

    row.appendChild(remove);

    subjectsContainer.appendChild(row);
}

// Initial subjects

addSubject();
addSubject();
addSubject();

document.getElementById("addSubject").onclick = addSubject;

// =====================================
// 3. SERVER CONNECTION STATUS
// =====================================

async function checkServer() {

    const status = document.getElementById("serverStatus");

    try {

        const response = await fetch("/");

        if (!response.ok) {
            throw new Error("Server unavailable");
        }

        status.textContent = "● C Server Connected";

    } catch (error) {

        status.textContent = "● C Server Disconnected";
    }
}

checkServer();

// =====================================
// 4. ENCODE DATA
// =====================================

function encodeData(data) {
    return new URLSearchParams(data).toString();
}

// =====================================
// 5. CALCULATE GPA
// =====================================

document.getElementById("calculate").onclick = async () => {

    const button = document.getElementById("calculate");

    try {

        const rows = [
            ...document.querySelectorAll(".subject-row")
        ];

        if (rows.length < 1 || rows.length > 10) {
            throw new Error("Enter 1 to 10 subjects.");
        }

        const data = {};

        const history =
            document.getElementById("history").value.trim();

        data.history = history;
        data.count = rows.length;

        // Validate subjects

        rows.forEach((row, index) => {

            const name =
                row.querySelector(".name").value.trim();

            const credits =
                row.querySelector(".credits").value.trim();

            const internal =
                row.querySelector(".internal").value.trim();

            const external =
                row.querySelector(".external").value.trim();

            // Allow zero marks

            if (
                name === "" ||
                credits === "" ||
                internal === "" ||
                external === ""
            ) {
                throw new Error(
                    `Fill all fields in subject ${index + 1}.`
                );
            }

            if (
                !Number.isInteger(Number(credits)) ||
                Number(credits) < 1 ||
                Number(credits) > 100
            ) {
                throw new Error(
                    `Credits must be a whole number between 1 and 100 in subject ${index + 1}.`
                );
            }

            if (
                Number(internal) < 0 ||
                Number(internal) > 50
            ) {
                throw new Error(
                    `Internal marks must be between 0 and 50 in subject ${index + 1}.`
                );
            }

            if (
                Number(external) < 0 ||
                Number(external) > 100
            ) {
                throw new Error(
                    `External marks must be between 0 and 100 in subject ${index + 1}.`
                );
            }

            data[`s${index}_name`] = name;
            data[`s${index}_credits`] = credits;
            data[`s${index}_internal`] = internal;
            data[`s${index}_external`] = external;
        });

        button.disabled = true;
        button.textContent = "Calculating...";

        // Send request to C backend

        const result = await fetchJSON("/api/calculate", {

            method: "POST",

            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded"
            },

            body: encodeData(data)
        });

        if (
            !Array.isArray(result.subjects) ||
            result.subjects.length === 0
        ) {
            throw new Error("Invalid subject data received from server.");
        }

        currentSubjects = result.subjects;

        // Display GPA

        document.getElementById("gpa").textContent =
            Number(result.gpa).toFixed(2);

        document.getElementById("cgpa").textContent =
            Number(result.cgpa).toFixed(2);

        document.getElementById("count").textContent =
            result.count;

        document.getElementById("prediction").textContent =
            Number(result.prediction).toFixed(2);

        document.getElementById("predictionInfo").textContent =
            "Calculated by C using linear regression.";

        // Display subject report

        const report = document.getElementById("report");

        report.replaceChildren();

        result.subjects.forEach(subject => {

            const row = document.createElement("tr");

            const values = [
                subject.name,
                subject.credits,
                Number(subject.total).toFixed(1),
                subject.grade,
                subject.point
            ];

            values.forEach(value => {

                const cell = document.createElement("td");

                cell.textContent = value;

                row.appendChild(cell);
            });

            report.appendChild(row);
        });

        document.getElementById("savedMessage").textContent =
            "✓ Record saved by C backend";

        document.getElementById("planner").replaceChildren();

        results.classList.remove("hidden");

        results.scrollIntoView({
            behavior: "smooth"
        });

    } catch (error) {

        alert(error.message);

        console.error("GPA calculation error:", error);

    } finally {

        button.disabled = false;

        button.textContent =
            "Calculate Academic Results";
    }
};

// =====================================
// 6. TARGET GPA PLANNER
// =====================================

document.getElementById("plan").onclick = async () => {

    if (!currentSubjects.length) {
        alert("Calculate GPA first.");
        return;
    }

    const target =
        document.getElementById("target").value;

    if (
        target.trim() === "" ||
        !Number.isFinite(Number(target)) ||
        Number(target) < 0 ||
        Number(target) > 10
    ) {
        alert("Enter a valid target GPA between 0 and 10.");
        return;
    }

    const button = document.getElementById("plan");

    try {

        button.disabled = true;

        const result = await fetchJSON("/api/plan", {

            method: "POST",

            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded"
            },

            body: encodeData({ target })
        });

        const planner =
            document.getElementById("planner");

        planner.replaceChildren();

        const heading = document.createElement("h3");

        heading.textContent =
            `Minimum total marks: ${result.threshold}`;

        planner.appendChild(heading);

        result.subjects.forEach(subject => {

            const item = document.createElement("div");

            item.className = "planner-item";

            item.textContent = subject.achievable

                ? `${subject.name}: ${Number(subject.required).toFixed(1)} / 100 external marks needed`

                : `${subject.name}: Target grade is not achievable`;

            planner.appendChild(item);
        });

        const note = document.createElement("p");

        note.style.color = "#94a3b8";
        note.style.fontSize = "12px";
        note.style.marginTop = "12px";

        note.textContent =
            "This estimates the external marks needed for the grade threshold associated with your target. It does not guarantee the exact semester GPA.";

        planner.appendChild(note);

    } catch (error) {

        alert(error.message);

        console.error("Planner error:", error);

    } finally {

        button.disabled = false;
    }
};

// =====================================
// 7. DOWNLOAD REPORT
// =====================================

document.getElementById("download").onclick = () => {

    if (!currentSubjects.length) {
        alert("Calculate GPA first.");
        return;
    }

    let content = "SMART STUDENT GPA REPORT\n";

    content += "==============================\n";

    content +=
        `Semester GPA: ${document.getElementById("gpa").textContent}\n`;

    content +=
        `Updated CGPA: ${document.getElementById("cgpa").textContent}\n\n`;

    currentSubjects.forEach(subject => {

        content += `${subject.name} | `;

        content += `Credits: ${subject.credits} | `;

        content += `Total: ${subject.total} | `;

        content += `Grade: ${subject.grade} | `;

        content += `GP: ${subject.point}\n`;
    });

    const blob = new Blob([content], {
        type: "text/plain"
    });

    const url = URL.createObjectURL(blob);

    const link = document.createElement("a");

    link.href = url;

    link.download = "Smart_GPA_Report.txt";

    link.click();

    URL.revokeObjectURL(url);
};