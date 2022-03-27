# include <Siv3D.hpp> // OpenSiv3D v0.6.1


struct Result {
    Array<bool>alive;
    Array<Array<double>> states;
    int32 score;
    Result(const int32 n): alive(n,true), states(n), score(0){}
};

struct FlappyBird{
    FlappyBird();
    
    const int32 State_n = 6;
    const int32 Action_n = 2;
    
    int32 player_n;
    
    Array<Vec2>Walls;
    Array<double> Player;
    Array<double> PlayerV;
    Array<bool> Alive;
    int32 score;
    
    FlappyBird(const int32 n):player_n(n),Player(n),PlayerV(n),Alive(n,true){
        
    }
    
    Result reset(){
        Walls.clear();
        Player.fill(300);
        PlayerV.fill(0);
        Alive.fill(true);
        Walls << Vec2(400, Random(200, 500));
        Walls << Vec2(800, Random(200, 500));
        score = 0;
        
        Array<Array<double>> state(player_n, {0.5, 0, Walls[0].x / 900.0, Walls[0].y / 600.0-0.5, Walls[1].x / 900.0, Walls[1].y / 600.0-0.5});
        
        Result res(player_n);
        res.states = state;
        
        return res;
    }
    
    Result step(Array<int32> actions){
        Result result(player_n);
        result.alive = Alive;
        
        if(Walls.front().x < 0) { // 障害物の削除 / 生成
            Walls.erase(Walls.begin());
            Walls << Vec2(800, Random(200, 500));
            ++score;
        }
        for(auto& i : Walls){
            i.x -= 5;
        }
        
        for(auto [idx, action] : Indexed(actions)){
            if(action) PlayerV[idx] = 10;
            PlayerV[idx] = Max(-10.0, PlayerV[idx] - 0.6); // 自由落下
            Player[idx] -= PlayerV[idx];
            
            const Circle Body = Circle(50, Player[idx], 10);
            
            if(not InRange(Player[idx], 0.0, 600.0)){
                result.alive[idx] = false;
            }
            if(Body.intersects(RectF(Walls.front(), 50, 1000)) or Body.intersects(RectF(Walls.front()-Vec2(0, 1100), 50, 1000))){
                result.alive[idx] = false;
            }
            
            result.states[idx] = {Player[idx] / 600.0, -PlayerV[idx] / 10.0, Walls[0].x / 900.0, (Walls[0].y-Player[idx]) / 600.0, Walls[0].x / 900.0, (Walls[0].y-Player[idx]) / 600.0 }; // 状況
        }
        
        Alive = result.alive;
        result.score = score;
        
        return result;
    }
    
    void draw() const{
        for(auto& i : Walls){
            RectF(i, 50, 1000).draw(Palette::Lightgreen);
            RectF(i-Vec2(0, 1100), 50, 1000).draw(Palette::Lightgreen);
        }
        
        for(auto [idx,i] : Indexed(Player)){
            if(Alive[idx]) Circle(50, i, 10).draw(Palette::Red);
        }
    }
};


struct Agent{
//    Agent();
    
    Array<double>param;

    // GAで調整
    int32 input = 6;
    int32 hidden = 6;
    int32 output = 2;
    int32 l1_size = (input+1) * hidden;
    int32 l2_size = (hidden+1) * output;
    int32 param_size = l1_size + l2_size;
    
    Agent(int32 hidden_):
    hidden(hidden_),
    l1_size((input+1) * hidden),
    l2_size((hidden+1) * output),
    param_size(l1_size + l2_size)
    {
        
        for(auto i : step(param_size)){
            param << Random() * 2 - 1;
        }
    }
    
    int32 predict (Array<double>& x){
        if(x.size() != input) Console << U"Invalid input size {}."_fmt(x.size());
        
        // 入力層
        Array<double>t1(hidden);
        for(auto jdx : step(hidden)){
            for(auto [idx, i] : Indexed(x)){
                t1[jdx] += i * param[jdx*(hidden+1)+idx];
            }
            t1[jdx] += param[jdx*(hidden+1)+hidden];
            if(t1[jdx] < 0) t1[jdx] = 0; // Reluで活性化
        }
        
        // 隠れ層
        Array<double>t2(output);
        for(auto jdx : step(output)){
            for(auto [idx, i] : Indexed(t1)){
                t2[jdx] += i * param[l1_size + jdx*(hidden+1)+idx];
            }
            t2[jdx] += param[l1_size + jdx*(hidden+1)+hidden];
        }
        
        // 評価値が最も高かったものを行動として返す
        int32 res = 0;
        double max = -Inf<double>;
        for(auto [idx, i] : Indexed(t2)) {
            if(max < i) {
                max = i;
                res = idx;
            }
        }
        return res;
    }
};


struct MainSys{
    MainSys();
    
    int32 n_agents;
    int32 wait;
    int32 score = 0;
    
    Array<Agent>agents;
    FlappyBird env;
    
    bool learn;
    
    MainSys(double n, int32 hidden):n_agents(n), env(n_agents), wait(17), learn(true){
        for(auto i : step(n_agents)){
            agents << Agent(hidden);
        }
    }
    
    void fit(double mutate, int32 n_elite){
        Result states(n_agents);
        int32 gen = 0;
        learn = true;
        while(learn){ // 永遠に学習()
            // まずは全員ゲームオーバーになるまで行動
            Array<int32>rank;
            states = env.reset();
            score = 0;

            while(states.alive.count(true) > 0){
                Array<int32>actions;
                for(int32 idx = 0; auto& a : agents){
                    if(states.alive[idx]) actions << a.predict(states.states[idx]);
                    else actions << 0;
                    ++idx;
                }
                Result result = env.step(actions);
                
                for(int32 idx : step(n_agents)){
                    if(result.alive[idx] == false && states.alive[idx] == true) {
                        rank << idx;
                    }
                }
                states = result;
                score = result.score;
                ClearPrint();
                Print << U"世代:{}, スコア:{}, 残存ユニット:{}"_fmt(gen, states.score, states.alive.count(true));
                System::Sleep(wait);
            }
            rank.reverse();
            
            // 優秀だったものを選ぶ
            Array<Array<double>>elite;
            
            for(auto i : step(n_elite)){
                elite << agents[rank[i]].param;
            }
            
            intersection(elite, mutate);
            
            Console << U"世代:{}, スコア:{}"_fmt(gen, states.score);
            ++gen;
        }
    }
    
    void intersection(Array<Array<double>>& elite, double mutate){
        const int32 length = elite[0].size();
        const int32 n_elite = elite.size();
        
        for(auto id : step(n_agents)){
            int32 Pos = Random(0, length); // どこで変えるか
            int32 target = 0;
            
            for(auto i : step(length)){
                if(Random() < mutate) agents[id].param[i] = Random() * 2 - 1; // 突然変異
                else{
                    if(i >= Pos) {
                        target = (target + 1) % n_elite;
                        Pos = Random(Pos, length);
                    }
                    agents[id].param[i] = elite[target][i];
                }
            }
        }
        
        // エリートは残す
        for(auto [idx, i] : Indexed(elite)){
            agents[idx].param = i;
        }
    }
    
    void draw() const{
        env.draw();
    }
    
    void switch_view() { // 人間に見える速度に変える
        wait = 17 - wait;
    }
    
    void terminate(){
        learn = false;
    }
    
    void save(){
        CSV csv;
        for(double i : agents[0].param){
            csv.write(i);
        }
        csv.save(U"params/"+DateTime::Now().format(U"yyyy-MM-dd_HH-mm-ss") + U"_" + ToString(score) + U".csv");
    }
    
    void draw_NN() const{
        // 入力層
        for(auto jdx : step(agents[0].hidden)){
            for(auto idx : step(agents[0].input)){
                double v = (1+agents[0].param[jdx*(agents[0].hidden+1)+idx]);
                Line(900, 450 + (idx-agents[0].input/2)*40, 1050, 450 + (jdx-agents[0].hidden/2)*40)
                .draw((v * v) * 1);
            }
            double v = (1+agents[0].param[jdx*(agents[0].hidden+1)+agents[0].hidden]);
            Line(900, 250, 1050, 450 + (jdx-agents[0].hidden/2)*40)
            .draw((v * v) * 1);
        }
        
        // 隠れ層
        for(auto jdx : step(agents[0].output)){
            for(auto idx : step(agents[0].hidden)){
                double v = (1+agents[0].param[agents[0].l1_size+jdx*(agents[0].output+1)+idx]);
                Line(1050, 450 + (idx-agents[0].hidden/2)*40, 1150, 450 + (jdx-agents[0].output/2)*40)
                .draw((v * v) * 1);
            }
            double v = (1+agents[0].param[agents[0].l1_size+jdx*(agents[0].output+1)+agents[0].output]);
            Line(1050, 250, 1150, 450 + (jdx-agents[0].output/2)*40)
            .draw((v * v) * 1);
        }
    }
};

void Main()
{
    Window::Resize(1200, 600);
    
    
    INI ini(U"config.ini");
    
    const double mutate = Parse<double>(ini[U"Main.mutate"]);
    const int32 n_agents = Parse<int32>(ini[U"Main.agent"]);
    const int32 n_elite = Parse<int32>(ini[U"Main.elite"]);
    const int32 a_hidden = Parse<int32>(ini[U"Agent.hidden"]);
    
    MainSys sys(n_agents, a_hidden);

    AsyncTask<void>task([&](){sys.fit(mutate, n_elite);});
    
	while (System::Update())
	{
        
        Line(800, 0, 800, 600).draw();
        {
            ScopedViewport2D viewport(Rect(0, 0, 800, 600));
            sys.draw();
        }
        sys.draw_NN();
        
        
        if(SimpleGUI::Button(U"Switch View", Vec2(900, 100))){
            sys.switch_view();
        }
        if(SimpleGUI::Button(U"Save Param1", Vec2(900, 150))){
            sys.save();
        }
        
	}
    sys.terminate();
    task.wait();
}
