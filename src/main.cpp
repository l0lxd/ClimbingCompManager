// Kletterwettkampf - Geruest. Server-seitig gerendertes HTML + HTMX, SQLite.
// Identitaet: der gekaufte Code. Mehrere Personen pro Geraet: Codes stehen
// kommagetrennt in der URL (/p/CODE1,CODE2), die URL ist also ein Lesezeichen.
// Hinweis: nicht kompiliert/getestet, bitte bei Crow-/SQLiteCpp-API-Fehlern melden.

#include <crow.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static bool valid_code(const std::string& s) {
    return !s.empty() && s.size() <= 32 &&
           std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
}

static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o += c;
        }
    }
    return o;
}

static std::vector<std::string> split_codes(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        if (valid_code(part) && std::find(out.begin(), out.end(), part) == out.end())
            out.push_back(part);
    }
    return out;
}

static std::string join_codes(const std::vector<std::string>& v) {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) o += (i ? "," : "") + v[i];
    return o;
}

static std::string page(const std::string& body) {
    return R"(<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kletterwettkampf</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css">
<script src="https://unpkg.com/htmx.org@2"></script>
<style>
 .row{display:flex;align-items:center;gap:.5rem;margin-bottom:.5rem}
 .row .nr{flex:1;font-size:1.2rem}
 .row button{width:3rem;height:3rem;padding:0;font-size:1.6rem;margin:0}
 .row button.topbtn{width:4.5rem;font-size:1rem;font-weight:bold}
 .row output{min-width:2rem;text-align:center;font-size:1.4rem}
 .row.top .nr{font-weight:bold}
 .bar{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;margin-bottom:1rem}
 .bar a,.bar label.plus{margin:0;padding:.5rem 1rem;border:1px solid var(--pico-muted-border-color);
   border-radius:.5rem;text-decoration:none;cursor:pointer}
 .bar a[aria-current]{background:var(--pico-primary-background);color:var(--pico-primary-inverse);border-color:transparent}
 .bar a.lb{border-color:var(--pico-primary);color:var(--pico-primary);font-weight:bold}
 tr.me td{font-weight:bold;background:var(--pico-primary-focus)}
 td.num,th.num{text-align:right}
 .bar .right{margin-left:auto;display:flex;gap:.5rem}
 .bar label.plus{font-weight:bold}
 #add,.addform{display:none}
 #add:checked ~ .addform{display:block}
</style></head><body><main class="container">)" + body + "</main></body></html>";
}

// Eine Routenzeile: [-] Versuche [+] [Top]. Jeder Tipp speichert sofort (kein Bestaetigen).
// Gewertet wird eine Route erst, wenn "Top" gedrueckt wurde.
static std::string row_html(const std::string& code, int route_id, int number, int attempts,
                            bool topped) {
    std::string id = "r-" + code + "-" + std::to_string(route_id);
    std::string base = "/r/" + code + "/" + std::to_string(route_id);
    std::ostringstream o;
    o << "<div class='row" << (topped ? " top" : "") << "' id='" << id << "'>"
      << "<span class='nr'>Route " << number << "</span>"
      << "<button class='secondary outline' hx-post='" << base << "/minus' hx-target='#" << id
      << "' hx-swap='outerHTML' aria-label='weniger'>&minus;</button>"
      << "<output>" << attempts << "</output>"
      << "<button class='secondary outline' hx-post='" << base << "/plus' hx-target='#" << id
      << "' hx-swap='outerHTML' aria-label='mehr'>+</button>"
      << "<button class='topbtn" << (topped ? "" : " outline") << "' hx-post='" << base
      << "/top' hx-target='#" << id << "' hx-swap='outerHTML' aria-pressed='"
      << (topped ? "true" : "false") << "'>" << (topped ? "&#10003; Top" : "Top") << "</button>"
      << "</div>";
    return o.str();
}

static int participant_id(SQLite::Database& db, const std::string& code) {
    SQLite::Statement q(db, "SELECT id FROM participant WHERE code = ?");
    q.bind(1, code);
    return q.executeStep() ? q.getColumn(0).getInt() : -1;
}

static crow::response redirect(const std::string& to) {
    crow::response r(303);
    r.add_header("Location", to);
    return r;
}

int main() {
    SQLite::Database db("kletter.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    {
        std::ifstream f("schema.sql");
        std::stringstream buf;
        buf << f.rdbuf();
        db.exec(buf.str());
    }

    crow::SimpleApp app;  // einzelner Thread: fuer einen Verein voellig ausreichend

    // Startseite: Code eingeben
    CROW_ROUTE(app, "/")([] {
        return crow::response(page(R"(
<h1>Kletterwettkampf</h1>
<form action="/go" method="get">
  <label>Dein Code
    <input name="code" autocomplete="off" autocapitalize="characters" required>
  </label>
  <button type="submit">Weiter</button>
</form>)"));
    });

    // Code einloesen bzw. weitere Person zu diesem Geraet hinzufuegen
    CROW_ROUTE(app, "/go")([&db](const crow::request& req) {
        const char* c = req.url_params.get("code");
        const char* have = req.url_params.get("codes");
        std::string code = c ? c : "";
        std::transform(code.begin(), code.end(), code.begin(),
                       [](unsigned char ch) { return std::toupper(ch); });
        std::vector<std::string> codes = split_codes(have ? have : "");
        if (!valid_code(code) || participant_id(db, code) < 0)
            return crow::response(404, page("<p>Code nicht gefunden.</p><a href='/'>Zurueck</a>"));
        if (std::find(codes.begin(), codes.end(), code) == codes.end()) codes.push_back(code);
        return redirect("/p/" + join_codes(codes));
    });

    // Name setzen (beim ersten Mal)
    CROW_ROUTE(app, "/n/<string>").methods("POST"_method)(
        [&db](const crow::request& req, const std::string& code) {
            if (!valid_code(code)) return crow::response(400);
            crow::query_string form("?" + req.body);
            const char* n = form.get("name");
            std::string name = n ? n : "";
            if (!name.empty() && name.size() <= 60) {
                SQLite::Statement q(db, "UPDATE participant SET name = ? WHERE code = ?");
                q.bind(1, name);
                q.bind(2, code);
                q.exec();
            }
            const char* back = req.url_params.get("back");
            return redirect(back ? std::string("/p/") + back : "/");
        });

    // Hauptseite: Tabs pro Person, darunter Routenliste
    CROW_ROUTE(app, "/p/<string>")([&db](const crow::request& req, const std::string& raw) {
        std::vector<std::string> codes = split_codes(raw);
        std::vector<std::string> valid;
        std::vector<std::string> names;
        for (const auto& c : codes) {
            SQLite::Statement q(db, "SELECT name FROM participant WHERE code = ?");
            q.bind(1, c);
            if (q.executeStep()) {
                valid.push_back(c);
                names.push_back(q.getColumn(0).isNull() ? "" : q.getColumn(0).getString());
            }
        }
        if (valid.empty()) return crow::response(404, page("<p>Keine gueltige Person.</p><a href='/'>Zurueck</a>"));

        size_t tab = 0;
        if (const char* t = req.url_params.get("tab")) {
            size_t v = static_cast<size_t>(std::atoi(t));
            if (v < valid.size()) tab = v;
        }
        std::string all = join_codes(valid);
        std::ostringstream o;
        // Checkbox + Label ersetzen JavaScript: "+" in der Leiste klappt das Formular darunter auf.
        o << "<input type='checkbox' id='add'>"
          << "<div class='bar'>";
        for (size_t i = 0; i < valid.size(); ++i) {
            std::string label = names[i].empty() ? valid[i] : esc(names[i]);
            o << "<a href='/p/" << all << "?tab=" << i << "'"
              << (i == tab ? " aria-current='page'" : "") << ">" << label << "</a>";
        }
        o << "<div class='right'>"
          << "<label for='add' class='plus' title='Person hinzufuegen'>+ Teilnehmer</label>"
          << "<a class='lb' href='/rangliste?codes=" << all << "'>Rangliste</a>"
          << "</div></div>"
          << "<form class='addform' action='/go' method='get'>"
          << "<input type='hidden' name='codes' value='" << all << "'>"
          << "<label>Code der neuen Person<input name='code' autocomplete='off' required></label>"
          << "<button type='submit'>Hinzufuegen</button></form>";

        const std::string& code = valid[tab];
        if (names[tab].empty()) {
            o << "<form method='post' action='/n/" << code << "?back=" << all << "'>"
              << "<label>Name<input name='name' required maxlength='60'></label>"
              << "<button type='submit'>Speichern</button></form>";
        } else {
            int pid = participant_id(db, code);
            SQLite::Statement q(db,
                "SELECT r.id, r.number, COALESCE(x.attempts, 0), COALESCE(x.topped, 0) "
                "FROM route r "
                "LEFT JOIN result x ON x.route_id = r.id AND x.participant_id = ? "
                "ORDER BY r.number");
            q.bind(1, pid);
            while (q.executeStep())
                o << row_html(code, q.getColumn(0).getInt(), q.getColumn(1).getInt(),
                              q.getColumn(2).getInt(), q.getColumn(3).getInt() != 0);
        }

        return crow::response(page(o.str()));
    });

    // Plus/Minus/Top: speichert sofort und liefert nur die neue Zeile zurueck (HTMX).
    // Regeln: Top braucht mindestens einen Versuch (bei 0 wird auf 1 gesetzt);
    // faellt die Zahl auf 0, ist die Route nicht mehr getoppt.
    CROW_ROUTE(app, "/r/<string>/<int>/<string>").methods("POST"_method)(
        [&db](const std::string& code, int route_id, const std::string& action) {
            int pid = participant_id(db, code);
            if (pid < 0 || (action != "plus" && action != "minus" && action != "top"))
                return crow::response(400);

            int attempts = 0;
            bool topped = false;
            {
                SQLite::Statement q(db,
                    "SELECT attempts, topped FROM result WHERE participant_id = ? AND route_id = ?");
                q.bind(1, pid);
                q.bind(2, route_id);
                if (q.executeStep()) {
                    attempts = q.getColumn(0).getInt();
                    topped = q.getColumn(1).getInt() != 0;
                }
            }
            if (action == "plus") {
                attempts = std::min(attempts + 1, 99);
            } else if (action == "minus") {
                attempts = std::max(attempts - 1, 0);
                if (attempts == 0) topped = false;
            } else {  // top
                topped = !topped;
                if (topped && attempts == 0) attempts = 1;
            }
            {
                SQLite::Statement q(db,
                    "INSERT INTO result (participant_id, route_id, attempts, topped) "
                    "VALUES (?, ?, ?, ?) "
                    "ON CONFLICT(participant_id, route_id) DO UPDATE SET "
                    "attempts = excluded.attempts, topped = excluded.topped, "
                    "updated_at = CURRENT_TIMESTAMP");
                q.bind(1, pid);
                q.bind(2, route_id);
                q.bind(3, attempts);
                q.bind(4, topped ? 1 : 0);
                q.exec();
            }
            int number = 0;
            {
                SQLite::Statement q(db, "SELECT number FROM route WHERE id = ?");
                q.bind(1, route_id);
                if (!q.executeStep()) return crow::response(404);
                number = q.getColumn(0).getInt();
            }
            return crow::response(row_html(code, route_id, number, attempts, topped));
        });

    // Rangliste: die meisten Tops zuerst, bei Gleichstand weniger Versuche.
    // (Die Wertungsregel wird spaeter ueber die Admin-Seite einstellbar.)
    // Zeigt nur Personen mit Namen, niemals die Codes. Eigene Personen (Codes in der URL)
    // werden hervorgehoben.
    CROW_ROUTE(app, "/rangliste")([&db](const crow::request& req) {
        const char* c = req.url_params.get("codes");
        std::vector<std::string> mine = split_codes(c ? c : "");
        std::string back = mine.empty() ? "/" : "/p/" + join_codes(mine);

        SQLite::Statement q(db,
            "SELECT p.code, p.name, rk.tops, rk.attempts "
            "FROM ranking rk JOIN participant p ON p.id = rk.id "
            "WHERE p.name IS NOT NULL AND p.name <> '' "
            "ORDER BY rk.tops DESC, rk.attempts ASC, p.name COLLATE NOCASE");

        std::ostringstream o;
        o << "<p><a href='" << back << "' role='button' class='secondary outline'>&larr; Zur&uuml;ck</a></p>"
          << "<h2>Rangliste</h2>";

        int position = 0, rank = 0, last_tops = -1, last_attempts = -1;
        std::ostringstream rows;
        while (q.executeStep()) {
            ++position;
            std::string code = q.getColumn(0).getString();
            std::string name = q.getColumn(1).getString();
            int tops = q.getColumn(2).getInt();
            int attempts = q.getColumn(3).getInt();
            if (tops != last_tops || attempts != last_attempts) rank = position;  // Gleichstand = gleicher Platz
            last_tops = tops;
            last_attempts = attempts;
            bool is_mine = std::find(mine.begin(), mine.end(), code) != mine.end();
            rows << "<tr" << (is_mine ? " class='me'" : "") << "><td class='num'>" << rank << "</td><td>"
                 << esc(name) << "</td><td class='num'>" << tops << "</td><td class='num'>"
                 << attempts << "</td></tr>";
        }
        if (position == 0) {
            o << "<p>Noch keine Eintr&auml;ge.</p>";
        } else {
            o << "<table><thead><tr><th class='num'>#</th><th>Name</th>"
              << "<th class='num'>Tops</th><th class='num'>Versuche</th></tr></thead><tbody>"
              << rows.str() << "</tbody></table>";
        }
        return crow::response(page(o.str()));
    });

    app.port(8080).run();
}
