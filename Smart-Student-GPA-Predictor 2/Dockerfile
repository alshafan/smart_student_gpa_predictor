FROM gcc:latest

WORKDIR /app

# Copy all files from the current folder into the container
COPY . .

# Compile main.c and gpa.c using clean relative paths
RUN gcc -Wall -Wextra -O2 -Ibackend backend/main.c backend/gpa.c -o server

# Render uses port 10000 by default
EXPOSE 10000

CMD ["./server"]
