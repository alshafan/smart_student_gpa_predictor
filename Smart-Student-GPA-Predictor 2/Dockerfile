FROM gcc:latest

WORKDIR /app

# Copy the entire project repository into the container
COPY . .

# Compile main.c and gpa.c together, using -I to locate headers inside the subfolder
RUN gcc -Wall -Wextra -O2 -I"Smart-Student-GPA-Predictor 2/backend" "Smart-Student-GPA-Predictor 2/backend/main.c" "Smart-Student-GPA-Predictor 2/backend/gpa.c" -o server

# Render uses port 10000 by default
EXPOSE 10000

CMD ["./server"]
