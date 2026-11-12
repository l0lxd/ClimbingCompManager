-- Nur zum Ausprobieren
INSERT OR IGNORE INTO settings (id, name, mode) VALUES (1, 'Vereinswettkampf', 'einzel');

INSERT OR IGNORE INTO route (number) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10);

INSERT OR IGNORE INTO participant (code) VALUES
  ('TEST1'),('TEST2'),('TEST3'),('TEST4'),('TEST5'),
  ('TEST6'),('TEST7'),('TEST8'),('TEST9'),('TEST10');