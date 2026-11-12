PRAGMA foreign_keys = ON;

-- Genau eine Zeile: der Wettkampf
CREATE TABLE IF NOT EXISTS settings (
  id    INTEGER PRIMARY KEY CHECK (id = 1),
  name  TEXT NOT NULL,
  mode  TEXT NOT NULL CHECK (mode IN ('einzel', 'gruppe'))
);

CREATE TABLE IF NOT EXISTS route (
  id     INTEGER PRIMARY KEY,
  number INTEGER NOT NULL UNIQUE,
  grade  TEXT
);

CREATE TABLE IF NOT EXISTS team (
  id   INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE COLLATE NOCASE
);

-- Code und Teilnehmer sind 1:1: der gekaufte Code ist der Datensatz.
-- Der Name wird beim ersten Einloggen gesetzt.
CREATE TABLE IF NOT EXISTS participant (
  id      INTEGER PRIMARY KEY,
  code    TEXT NOT NULL UNIQUE,
  name    TEXT,
  team_id INTEGER REFERENCES team(id) ON DELETE SET NULL
);

-- attempts: gezaehlte Versuche. Gewertet wird die Route erst, wenn topped = 1.
CREATE TABLE IF NOT EXISTS result (
  participant_id INTEGER NOT NULL REFERENCES participant(id) ON DELETE CASCADE,
  route_id       INTEGER NOT NULL REFERENCES route(id) ON DELETE CASCADE,
  attempts       INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),
  topped         INTEGER NOT NULL DEFAULT 0 CHECK (topped IN (0, 1)),
  updated_at     TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (participant_id, route_id)
);

-- Ranking: mehr Tops zuerst, bei Gleichstand weniger Versuche.
-- Nur Routen mit topped = 1 zaehlen, auch fuer die Versuchssumme.
CREATE VIEW IF NOT EXISTS ranking AS
SELECT p.id, p.name, p.team_id,
       COUNT(r.route_id)             AS tops,
       COALESCE(SUM(r.attempts), 0)  AS attempts
FROM participant p
LEFT JOIN result r ON r.participant_id = p.id AND r.topped = 1
GROUP BY p.id;

-- Gruppe verlassen (kommt als naechstes, nach dem Warn-Popup):
--   DELETE FROM result WHERE participant_id = ?;
--   UPDATE participant SET team_id = NULL WHERE id = ?;
--   DELETE FROM team WHERE id = ? AND NOT EXISTS
--     (SELECT 1 FROM participant WHERE team_id = team.id);