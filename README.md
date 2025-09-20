long duracion;
int distancia;

#define velocidad 50

// Pines del sensor HC-SR04
#define TRIG 2
#define ECHO 3

// Pines del L298N
#define IN1 6  // Motor A
#define IN2 9
#define IN3 10   // Motor B
#define IN4 5

void setup() {
  Serial.begin(9600);

  // Sensor
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Motores
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void loop() {
  // --- Medición de distancia ---
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  duracion = pulseIn(ECHO, HIGH);
  distancia = duracion * 0.034 / 2; // cm

  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");
  delay(50);
  
  if (distancia > 15) {
    // Ambos motores adelante
    analogWrite(IN1, 0);
    analogWrite(IN2, velocidad);
    analogWrite(IN3, 0);
    analogWrite(IN4, velocidad);
  } 
  else {
    // Motor A adelante
    analogWrite(IN1, velocidad);
    analogWrite(IN2, 0);
    // Motor B atrás
    analogWrite(IN3, 0);
    analogWrite(IN4, velocidad);
    analogWrite(IN1, velocidad);
    analogWrite(IN2, 0);
    analogWrite(IN3, 0);
    analogWrite(IN4, velocidad);
  }

  delay(100);
}
