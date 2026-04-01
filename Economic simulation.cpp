# include <Siv3D.hpp>

// --- 定数・列挙型 ---
enum class GameState { Title, Playing, GameOver, GameClear };

// --- パーティクルシステム ---
struct Particle {
	Vec2 pos;
	Vec2 velocity;
	double life = 1.0;
	Color color;
};

struct ParticleManager {
	Array<Particle> particles;
	void emit(Vec2 pos, int32 count, Color color) {
		for (int32 i = 0; i < count; ++i)
			particles << Particle{ pos, RandomVec2(Circle{150}), Random(0.5, 1.2), color };
	}
	void update(double dt) {
		for (auto& p : particles) { p.pos += p.velocity * dt; p.velocity *= 0.95; p.life -= dt; }
		particles.remove_if([](const Particle& p) { return p.life <= 0; });
	}
	void draw() const {
		for (const auto& p : particles) Circle{ p.pos, 4 * p.life }.draw(ColorF{ p.color, p.life });
	}
};

// --- イベントシステム ---
struct GameEvent {
	String name = U"平常時";
	double stockMultiplier = 1.0;
	double prodMultiplier = 1.0;
	double timeLeft = 0.0;
	Color color = Palette::White;
};

// --- 経済データ管理 ---
struct Economy {
	double stockPrice = 100.0;
	double money = 2000.0;
	int32 ownedStocks = 0;
	int32 population = 100;
	double resourceStock = 50.0;
	int32 powerPlants = 0;
	double plantCost = 500.0;
	double targetAssets = 1000000.0;
	int32 currentLevel = 0;
	GameEvent currentEvent;

	void reset(int32 level) {
		currentLevel = level;
		stockPrice = 100.0; ownedStocks = 0; population = 100;
		resourceStock = 100.0; powerPlants = 0; plantCost = 500.0;
		currentEvent = { U"平常時", 1.0, 1.0, 0.0, Palette::White };
		if (level == 0) { money = 3000.0; targetAssets = 500000.0; }
		else { money = 1500.0; targetAssets = 2000000.0; }
	}

	double getTotalAssets() const { return money + (ownedStocks * stockPrice); }

	void update(double dt) {
		// イベント更新
		if (currentEvent.timeLeft > 0) {
			currentEvent.timeLeft -= dt;
			if (currentEvent.timeLeft <= 0) currentEvent = { U"平常時", 1.0, 1.0, 0.0, Palette::White };
		}
		else if (Random() <= 0.001) {
			int32 type = Random(0, 2);
			if (type == 0) currentEvent = { U"🔥 技術革新！(生産2倍)", 1.0, 2.5, 10.0, Palette::Limegreen };
			else if (type == 1) currentEvent = { U"📉 暗黒の月曜日 (暴落)", 6.0, 1.0, 8.0, Palette::Tomato };
			else currentEvent = { U"🚀 経済バブル (上昇)", 2.5, 1.2, 12.0, Palette::Gold };
		}

		// 経済計算
		double bias = (population - 100) * 0.001;
		stockPrice = Max(1.0, stockPrice + (Random(-1.0, 1.1) * currentEvent.stockMultiplier + bias) * dt * 15);
		resourceStock += (powerPlants * 6.0 * currentEvent.prodMultiplier) * dt;

		if (resourceStock > 0) {
			population += static_cast<int32>(population * 0.01 * dt + Random());
			resourceStock -= population * 0.006 * dt;
		}
		else {
			population -= static_cast<int32>((currentLevel == 0 ? 10 : 30) * dt + 1);
		}
		resourceStock = Max(0.0, resourceStock);
	}
};

// --- セーブ機能 ---
void SaveBestTimes(const double times[2]) {
	TextWriter writer{ U"save.txt" };
	if (writer) { writer.writeln(times[0]); writer.writeln(times[1]); }
}
void LoadBestTimes(double times[2]) {
	TextReader reader{ U"save.txt" };
	if (reader) {
		String line;
		if (reader.readLine(line)) times[0] = ParseOr<double>(line, 9999.9);
		if (reader.readLine(line)) times[1] = ParseOr<double>(line, 9999.9);
	}
}

// --- メイン関数 ---
void Main() {
	Scene::SetBackground(Palette::Darkslategray);
	const Font font{ FontMethod::MSDF, 25 };
	const Font titleFont{ FontMethod::MSDF, 60, Typeface::Bold };

	// サウンド
	const Audio seCoin{ GMInstrument::TinkleBell, 80, 0.2s };
	const Audio seClick{ GMInstrument::Woodblock, 70, 0.1s };
	const Audio seClear{ GMInstrument::Trumpet, 72, 1.0s };

	Economy economy;
	GameState state = GameState::Title;
	ParticleManager pm;
	Stopwatch stopwatch;
	double bestTimes[2] = { 9999.9, 9999.9 };
	Array<double> priceHistory;

	LoadBestTimes(bestTimes);

	while (System::Update()) {
		double dt = Scene::DeltaTime();
		pm.update(dt);

		if (state == GameState::Title) {
			titleFont(U"経済タイムアタック").drawAt(Scene::Center().movedBy(0, -120));
			font(U"BEST [Easy]: {:.2f}s  [Hard]: {:.2f}s"_fmt(
				bestTimes[0] >= 9000 ? 0 : bestTimes[0],
				bestTimes[1] >= 9000 ? 0 : bestTimes[1])).drawAt(Scene::Center().movedBy(0, -40), Palette::Gold);

			if (SimpleGUI::Button(U"Easyモード (目標50万)", Scene::Center().movedBy(-120, 60), 220)) {
				seClick.playOneShot(); economy.reset(0); state = GameState::Playing; stopwatch.restart(); priceHistory.clear();
			}
			if (SimpleGUI::Button(U"Hardモード (目標200万)", Scene::Center().movedBy(120, 60), 220)) {
				seClick.playOneShot(); economy.reset(1); state = GameState::Playing; stopwatch.restart(); priceHistory.clear();
			}
		}
		else {
			economy.update(dt);
			if (state == GameState::Playing) {
				priceHistory << economy.stockPrice;
				if (priceHistory.size() > 400) priceHistory.pop_front();

				// クリア判定
				if (economy.getTotalAssets() >= economy.targetAssets) {
					state = GameState::GameClear; stopwatch.pause(); seClear.playOneShot();
					if (stopwatch.sF() < bestTimes[economy.currentLevel]) {
						bestTimes[economy.currentLevel] = stopwatch.sF();
						SaveBestTimes(bestTimes);
					}
				}
				// ゲームオーバー判定
				if (economy.population <= 0) { state = GameState::GameOver; stopwatch.pause(); }
			}

			// UI描画
			font(U"TIME: {:.2f}s"_fmt(stopwatch.sF())).draw(20, 20, Palette::Yellow);
			font(U"総資産: {:.0f} / 目標: {:.0f}"_fmt(economy.getTotalAssets(), economy.targetAssets)).draw(20, 60);
			font(U"人口: {}人 / 資源: {:.1f}"_fmt(economy.population, economy.resourceStock)).draw(20, 100);

			// イベント表示
			Rect{ 20, 150, 350, 50 }.draw(ColorF{ 1, 1, 1, 0.1 });
			font(economy.currentEvent.name).draw(30, 155, economy.currentEvent.color);

			// グラフ
			for (size_t i = 1; i < priceHistory.size(); ++i)
				Line{ 450 + (i - 1), 400 - priceHistory[i - 1] * 0.3, 450 + i, 400 - priceHistory[i] * 0.3 }.draw(2, economy.currentEvent.color);

			if (state == GameState::Playing) {
				if (SimpleGUI::Button(U"1株買う ({:.1f})"_fmt(economy.stockPrice), Vec2{ 20, 220 }, 200)) {
					if (economy.money >= economy.stockPrice) { economy.money -= economy.stockPrice; economy.ownedStocks++; seClick.playOneShot(0.3); }
				}
				if (SimpleGUI::Button(U"1株売る", Vec2{ 230, 220 }, 150)) {
					if (economy.ownedStocks > 0) {
						economy.money += economy.stockPrice; economy.ownedStocks--;
						pm.emit(Vec2{ 300, 235 }, 15, Palette::Gold); seCoin.playOneShot(0.6);
					}
				}
				if (SimpleGUI::Button(U"発電所建設 ({:.0f} JPY)"_fmt(economy.plantCost), Vec2{ 20, 280 }, 360)) {
					if (economy.money >= economy.plantCost) {
						economy.money -= economy.plantCost; economy.powerPlants++; economy.plantCost *= 1.3;
						seClick.playOneShot(); pm.emit(Vec2{ 200, 295 }, 10, Palette::Cyan);
					}
				}
			}
			else {
				Rect{ Scene::Size() }.draw(ColorF{ 0, 0, 0, 0.7 });
				titleFont(state == GameState::GameClear ? U"CLEAR!" : U"GAME OVER").drawAt(Scene::Center(), state == GameState::GameClear ? Palette::Gold : Palette::Red);
				if (SimpleGUI::Button(U"タイトルへ", Scene::Center().movedBy(-70, 120))) state = GameState::Title;
				if (state == GameState::GameClear && Random() <= 0.001) pm.emit(Scene::Center(), 5, HSV{ Random(0, 360), 0.8, 1.0 });
			}
		}
		pm.draw();
	}
}
