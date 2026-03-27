#define _CRT_SECURE_NO_WARNINGS
#include<graphics.h>
#include<conio.h>
#include<Windows.h>
#include<stdio.h>
#include<math.h>
#define RANK_FILE "scores.dat"
#define RANK_SIZE 5
struct RankEntry {
	char name[22];   // 最多21字符 + '\0'
	int  score;
};
RankEntry rankList[RANK_SIZE];

bool roles = 0, isstart = 0;//默认角色为奇犽
int difficulties = -1;//默认难度为简单
char* rank[5];
bool lastad = 1;//默认右 储存上一布的朝向
const int GROUND_Y = 670;  // 地面Y坐标 
const int LEFT_BOUND = 0;
const int RIGHT_BOUND = 1000; // 右边留200px给Boss站位
const float GRAVITY = 0.8f;   // 每帧重力加速度
const float JUMP_POWER = -16.0f; // 起跳初速度（负数=向上）
int   gameFrames = 0;    // 游戏运行帧数（每帧+1）
int   finalScore = 0;    // 最终得分（结算时计算）
bool  gameWin = false;
bool  gameLose = false;


void loadRank()
{
	memset(rankList, 0, sizeof(rankList));
	FILE* f = fopen(RANK_FILE, "rb");
	if (!f) return;
	fread(rankList, sizeof(RankEntry), RANK_SIZE, f);
	fclose(f);
}

void saveRank(int newScore,const char * name)
{
	loadRank();
	// 找最低分位置替换
	int minIdx = - 1;
	for (int i = 0; i < RANK_SIZE; i++) {
		if (newScore > rankList[i].score) { minIdx = i; break; }
	}
	if (minIdx == -1) return;
	// 后移腾位
	for (int i = RANK_SIZE - 1; i > minIdx; i--)
		rankList[i] = rankList[i - 1];
	// 插入新记录
	strncpy(rankList[minIdx].name, name, 21);
	rankList[minIdx].name[21] = '\0';
	rankList[minIdx].score = newScore;

	FILE* f = fopen(RANK_FILE, "wb");
	if (!f) return;
	fwrite(rankList, sizeof(RankEntry), RANK_SIZE, f);
	fclose(f);
}

void inputNickname(char* outName)
{
	memset(outName, 0, 22);
	int len = 0;
	bool confirmed = false;

	ExMessage dummy;
	while (peekmessage(&dummy, EX_KEY)); // 清空残留消息

	while (!confirmed)
	{
		BeginBatchDraw();

		setfillcolor(RGB(30, 30, 60));
		solidrectangle(340, 420, 940, 560);
		setcolor(RGB(100, 180, 255));
		rectangle(340, 420, 940, 560);

		settextstyle(24, 0, _T("黑体"));
		settextcolor(RGB(200, 220, 255));
		setbkmode(TRANSPARENT);
		outtextxy(360, 435, _T("Enter a nickname (A-Z 0-9 _, max 21):"));
		outtextxy(360, 530, _T("Enter = confirm    ESC = Anonymous"));

		setfillcolor(RGB(20, 20, 40));
		solidrectangle(360, 470, 920, 510);
		setcolor(RGB(100, 180, 255));
		rectangle(360, 470, 920, 510);

		// 光标闪烁：用系统时间控制，不依赖 gameFrames
		TCHAR display[48] = { 0 };
		for (int i = 0; i < len; i++) display[i] = (TCHAR)outName[i];
		display[len] = (GetTickCount() / 400) % 2 == 0 ? '_' : ' ';
		settextstyle(22, 0, _T("Courier New"));
		settextcolor(WHITE);
		outtextxy(368, 480, display);

		TCHAR countBuf[16];
		_stprintf_s(countBuf, _T("%d/21"), len);
		settextstyle(18, 0, _T("黑体"));
		settextcolor(len == 21 ? RGB(255, 80, 80) : RGB(150, 150, 150));
		outtextxy(860, 535, countBuf);

		EndBatchDraw();

		// ---- 改用 WM_KEYDOWN + GetKeyState 判断字符 ----
		ExMessage msg;
		if (peekmessage(&msg, EX_KEY) && msg.message == WM_KEYDOWN)
		{
			UINT vk = msg.vkcode;

			if (vk == VK_RETURN)                          // Enter
			{
				if (len == 0) strncpy(outName, "Anonymous", 21);
				confirmed = true;
			}
			else if (vk == VK_ESCAPE)                     // ESC
			{
				strncpy(outName, "Anonymous", 21);
				confirmed = true;
			}
			else if (vk == VK_BACK && len > 0)            // Backspace
			{
				outName[--len] = '\0';
			}
			else if (len < 21)
			{
				bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
				char ch = 0;

				if (vk >= 'A' && vk <= 'Z') {
					// Shift 或 CapsLock 决定大小写
					ch = (shift ^ caps) ? (char)vk : (char)(vk + 32);
				}
				else if (vk >= '0' && vk <= '9') {
					ch = shift ? 0 : (char)vk; // Shift+数字是符号，忽略
				}
				else if (vk == VK_OEM_MINUS && shift) {   // Shift+- = _
					ch = '_';
				}

				if (ch != 0) {
					outName[len++] = ch;
					outName[len] = '\0';
				}
			}
		}

		Sleep(10);
	}
}

//有关initmenu的所有函数：
void tool() { //获取位置工具
	if (GetAsyncKeyState(VK_RBUTTON))
	{
		POINT pt;
		GetCursorPos(&pt);
		RECT rect;
		HWND hwnd = GetHWnd();
		GetWindowRect(hwnd, &rect);
		printf("x:%d y:%d\n", pt.x - rect.left, pt.y - rect.top);//通过右键单击获取按钮坐标
	}
}

void srad()//select roles and difficulties and whether game start
{
	if (GetAsyncKeyState(VK_LBUTTON))
	{
		POINT pt;
		GetCursorPos(&pt);
		RECT rect;
		HWND hwnd = GetHWnd();
		GetWindowRect(hwnd, &rect);
		int tx = pt.x - rect.left, ty = pt.y - rect.top;
		if (tx >= 525 && tx <= 628 && ty >= 518 && ty <= 623) roles = 0;
		if (tx >= 696 && tx <= 800 && ty >= 516 && ty <= 623) roles = 1;
		if (tx >= 442 && tx <= 540 && ty >= 218 && ty <= 274) difficulties = -1;
		if (tx >= 614 && tx <= 719 && ty >= 219 && ty <= 267) difficulties = 0;
		if (tx >= 786 && tx <= 891 && ty >= 219 && ty <= 267) difficulties = 1;
		if (tx >= 520 && tx <= 808 && ty >= 700 && ty <= 772) isstart = true;
	}
	//tool(); 需要坐标再调用
}

void ss() //showselection
{
	setfillcolor(RED);
	fillcircle(570 + 168 * (int)roles, 608, 5);
	fillcircle(490 + 173 * (difficulties + 1), 255, 5);
}

void sps()//show pending selection
{
	setfillcolor(WHITE);
	fillcircle(570 + 168, 608, 5);
	fillcircle(570, 608, 5);
	fillcircle(490, 255, 5);
	fillcircle(490 + 173, 255, 5);
	fillcircle(490 + 173 * 2, 255, 5);
}

void sr() // showrank
{
	loadRank();
    setbkmode(TRANSPARENT);
    for (int i = 0; i < RANK_SIZE; i++) {
        TCHAR buf[48];
        if (rankList[i].score > 0) {
            // 昵称转宽字符显示
            TCHAR wname[22] = {0};
            for (int j = 0; j < 21 && rankList[i].name[j]; j++)
                wname[j] = (TCHAR)(unsigned char)rankList[i].name[j];
            _stprintf_s(buf, _T("%-10s %d"), wname, rankList[i].score);
        } else {
            _stprintf_s(buf, _T("---"));
        }
        settextcolor(i == 0 ? RGB(255,215,0) : BLACK);
        settextstyle(25, 0, _T("Courier New"));   // 等宽字体让对齐更好看
        outtextxy(1000, 313 + 64 * i, buf);
    }
}

void initmenu()
{
	initgraph(1280, 750);
	IMAGE background;
	loadimage(&background, _T("Hunter-X-Hunter.jpg"));
	putimage(0, 0, &background);
	fillcircle(570 + 168 * (int)roles, 608, 5);
	fillcircle(490 + 173 * (difficulties + 1), 255, 5);
	//
	setbkmode(TRANSPARENT);
	settextstyle(30, 0, _T("黑体"));
	settextcolor(RGB(113, 109, 149));
	outtextxy(972, 719, _T("按ESC键退出游戏"));
	sr();
	while (!GetAsyncKeyState(27))
	{
		BeginBatchDraw();
		srad();
		sps();
		ss();
		Sleep(10);
		EndBatchDraw();
		if(isstart) break;
	}
	closegraph();
}

//有关游戏主体的有关函数：

struct SkillData {
	int castFrames;    // 前摇帧数（期间不能移动）
	int damage;        // 命中伤害
	int cooldown;      // 冷却帧数
};

SkillData qySkills[3] = {
	{20, 25, 180},   // 技能1：前摇短、伤害中、冷却3秒
	{40, 45, 300},   // 技能2：前摇长、伤害高、冷却5秒
	{10, 15, 120},   // 技能3：前摇很短、伤害低、冷却2秒
};

// 小杰三个技能
SkillData xjSkills[3] = {
	{15, 20, 150},   // 技能1
	{50, 60, 360},   // 技能2：大招，前摇最长
	{25, 30, 200},   // 技能3
};

//特效

struct Effect {
	int x, y;
	int timer;      // 剩余显示帧数
	int type;       // 0=普攻 1=技能1 2=技能2 3=技能3
	bool active;
};
const int MAX_EFFECT = 8;
Effect effects[MAX_EFFECT];

void spawnEffect(int x, int y, int type)
{
	for (int i = 0; i < MAX_EFFECT; i++) {
		if (!effects[i].active) {
			effects[i] = { x, y, 18, type, true };
			return;
		}
	}
}

void drawEffects()
{
	for (int i = 0; i < MAX_EFFECT; i++) {
		if (!effects[i].active) continue;
		int alpha = effects[i].timer * 14;   // 越来越淡
		int r = effects[i].timer * 3;        // 扩散半径
		switch (effects[i].type) {
		case 0: // 普攻：白色扩散圆
			setcolor(RGB(255, 255, 200));
			circle(effects[i].x, effects[i].y, r);
			break;
		case 1: // 技能1：青色圆环
			setcolor(RGB(0, 220, 255));
			circle(effects[i].x, effects[i].y, r * 2);
			circle(effects[i].x, effects[i].y, r);
			break;
		case 2: // 技能2：金色大爆炸
			setfillcolor(RGB(255, 200, 0));
			solidcircle(effects[i].x, effects[i].y, r * 2);
			setfillcolor(RGB(255, 255, 255));
			solidcircle(effects[i].x, effects[i].y, r);
			break;
		case 3: // 技能3：紫色闪光
			setcolor(RGB(180, 0, 255));
			for (int j = 0; j < 6; j++) {
				float ang = j * 3.14159f / 3.0f;
				line(effects[i].x, effects[i].y,
					effects[i].x + (int)(cosf(ang) * r * 2),
					effects[i].y + (int)(sinf(ang) * r * 2));
			}
			break;
		}
		effects[i].timer--;
		if (effects[i].timer <= 0) effects[i].active = false;
	}
}

struct hero {
	int locx;
	int locy;
	int totalhp;
	int lefthp;
	bool skills[3];
	bool survive;
	float velY;
	int jumpcount;
	bool onground;
	bool iscrouching;
	int  attackTimer;    // 普通攻击冷却计时（>0说明在冷却）
	int  skillTimer[3];  // 三个技能各自的冷却计时
	int  castTimer;      // 前摇计时（>0说明在前摇中，不能移动）
	int  currentSkill;   // 当前正在释放的技能编号（-1=无）
	bool isAttacking;    // 是否在普通攻击动作中
	int hitFlashTimer;   // 受击闪烁计时（>0时显示红色）
}qy, xj;

struct herostates {
	IMAGE stand[2];   // [0]=左 [1]=右
	IMAGE jump[2];
	IMAGE crouch[2];
}qysta,xjsta;


struct Bullet {
	float x, y;
	float vx, vy;
	bool active;
};
const int MAX_BULLET = 10;//最大十个子弹

struct Boss {
	int locx, locy;
	int totalhp;
	int lefthp;
	bool survive;
	int phase;//血量关联状态；
	int attacktimer;//攻击计时器
	int attackInterval; //血量关联的攻击间隔
	int damage; //伤害
	int bulletPattern;   // 0=直线 1=散射 2=追踪
	Bullet bullets[MAX_BULLET];//子弹库；
}boss;

// 得分公式：基础分 + 血量奖励 - 时间惩罚
// 基础分1000，每1hp加10分，每秒超过30秒扣5分
int calcScore(hero* h)
{
	int timeSeconds = gameFrames / 60;        // 帧数÷60≈秒数（Sleep10ms≈100帧/秒，按实际调整）
	int score = 1000;
	score += h->lefthp * 10;                  // 血量奖励
	int overTime = max(0, timeSeconds - 30);  // 超过30秒开始扣分
	score -= overTime * 5;
	if (score < 0) score = 0;
	return score;
}

int checkGameOver(hero* h)//是否胜利
{
	if (!boss.survive) {
		finalScore = calcScore(h);
		gameWin = true;
		return 1;
	}
	if (!h->survive || h->lefthp <= 0) {
		finalScore = 0;   // 失败不计分
		gameLose = true;
		return 2;
	}
	return 0;
}

void handleAttack(hero* h)
{
	// ---- 所有计时器每帧-1 ----
	if (h->attackTimer > 0) h->attackTimer--;
	if (h->castTimer > 0) h->castTimer--;
	for (int i = 0; i < 3; i++)
		if (h->skillTimer[i] > 0) h->skillTimer[i]--;

	// 前摇期间不处理新输入
	if (h->castTimer > 0) return;

	// ---- 前摇结束时结算伤害 ----
	if (h->currentSkill >= 0) {
		// 取当前角色的技能表
		SkillData* sd = roles ? xjSkills : qySkills;
		// 命中判定：Boss必须在攻击范围内（站在Boss左侧附近）
		int dist = boss.locx - h->locx;
		if (dist > 0 && dist < 300) {  // 300px内视为命中
			boss.lefthp -= sd[h->currentSkill].damage;
			if (boss.lefthp < 0) boss.lefthp = 0;
			if (boss.lefthp == 0) boss.survive = false;
		}
		h->currentSkill = -1;
	}
	if (h->isAttacking) h->isAttacking = false;

	// ---- J键普通攻击 ----
	bool keyJ = (GetAsyncKeyState('J') & 0x8000) != 0;
	if (keyJ && h->attackTimer == 0) {
		h->attackTimer = 30;   // 0.5秒冷却
		h->castTimer = 10;   // 前摇10帧
		h->currentSkill = -2;   // -2表示普通攻击（特殊标记）
		h->isAttacking = true;

		// 普通攻击前摇极短，直接在这里结算也可以
		int dist = boss.locx - h->locx;
		spawnEffect(h->locx + 80, h->locy + 40, 0);
		if (dist > 0 && dist < 150) {  // 普通攻击范围更短
			boss.lefthp -= 8;
			if (boss.lefthp < 0) boss.lefthp = 0;
			if (boss.lefthp == 0) boss.survive = false;
			
		}
		h->currentSkill = -1;
		return;
	}

	// ---- 1/2/3技能 ----
	SkillData* sd = roles ? xjSkills : qySkills;
	for (int i = 0; i < 3; i++) {
		bool keyPressed = (GetAsyncKeyState('1' + i) & 0x8000) != 0;
		if (keyPressed && h->skillTimer[i] == 0) {
			h->castTimer = sd[i].castFrames;
			h->skillTimer[i] = sd[i].cooldown;
			h->currentSkill = i;
			spawnEffect(h->locx + 80, h->locy + 40, h->currentSkill + 1);
			break; // 同一帧只释放一个技能
		}
	}
}

void inithero()
{
	qy.totalhp = 100;
	qy.lefthp = 100;
	qy.survive = true;
	qy.locx = 220;
	qy.locy = GROUND_Y;
	qy.velY = 0; qy.jumpcount = 0; qy.onground = true; qy.iscrouching = false;
	for (int i = 0; i < 3; i++) qy.skills[i] = true;
	xj.totalhp = 100;
	xj.lefthp = 100;
	xj.survive = true;
	xj.locx = 220;
	xj.locy = GROUND_Y;
	xj.velY = 0; xj.jumpcount = 0; xj.onground = true; xj.iscrouching = false;
	for (int i = 0; i < 3; i++) xj.skills[i] = true;
	qy.attackTimer = 0;
	qy.castTimer = 0;
	qy.currentSkill = -1;
	qy.isAttacking = false;
	for (int i = 0; i < 3; i++) qy.skillTimer[i] = 0;
	xj.attackTimer = 0;
	xj.castTimer = 0;
	xj.currentSkill = -1;
	xj.isAttacking = false;
	for (int i = 0; i < 3; i++) xj.skillTimer[i] = 0;
	qy.hitFlashTimer = 0;
	xj.hitFlashTimer = 0;
}

void initboss()
{
	boss.locx = 900;           // Boss 固定在右侧
	boss.locy = GROUND_Y;
	boss.totalhp = 100;
	boss.lefthp = 100;
	boss.survive = true;
	boss.phase = 0;
	boss.attacktimer = 60;     // 先给60帧后再开始攻击

	// ---- 根据难度设置 Boss 参数 ----
	// difficulties: -1=简单  0=中等  1=困难
	if (difficulties == -1) {
		boss.attackInterval = 120; // 简单：2秒攻击一次
		boss.damage = 5;
	}
	else if (difficulties == 0) {
		boss.attackInterval = 80;  // 中等：约1.3秒
		boss.damage = 10;
	}
	else {
		boss.attackInterval = 50;  // 困难：约0.8秒
		boss.damage = 15;
	}

	// 清空所有弹幕
	for (int i = 0; i < MAX_BULLET; i++)
		boss.bullets[i].active = false;
}
void initgame()//游戏初始化
{
	inithero();
	initboss();
}

void bossAI(hero* h)
{
	if (!boss.survive) return;

	// ---- 阶段变化（影响攻击间隔和速度）----
	int hpPercent = boss.lefthp * 100 / boss.totalhp;
	if (hpPercent <= 30 && boss.phase < 2) {
		boss.phase = 2;
		boss.attackInterval = max(30, boss.attackInterval - 20); // 濒死狂暴
	}
	else if (hpPercent <= 60 && boss.phase < 1) {
		boss.phase = 1;
		boss.attackInterval = max(40, boss.attackInterval - 10); // 半血加速
	}

	// ---- 攻击计时 ----
	boss.attacktimer--;

	// ---- 根据阶段切换弹幕模式 ----
	boss.bulletPattern = boss.phase;   // 阶段0→直线，1→散射，2→追踪

	if (boss.attacktimer <= 0) {
		boss.attacktimer = boss.attackInterval;

		if (boss.bulletPattern == 0) {
			// 模式0：单发直线
			for (int i = 0; i < MAX_BULLET; i++) {
				if (!boss.bullets[i].active) {
					boss.bullets[i].x = (float)boss.locx;
					boss.bullets[i].y = (float)(boss.locy + 40);
					boss.bullets[i].vx = -8.0f;
					boss.bullets[i].vy = 0.0f;   // 新增vy字段，见下方
					boss.bullets[i].active = true;
					break;
				}
			}
		}
		else if (boss.bulletPattern == 1) {
			// 模式1：三连散射（上/中/下）
			float angles[3] = { -0.3f, 0.0f, 0.3f };
			int shot = 0;
			for (int i = 0; i < MAX_BULLET && shot < 3; i++) {
				if (!boss.bullets[i].active) {
					boss.bullets[i].x = (float)boss.locx;
					boss.bullets[i].y = (float)(boss.locy + 40);
					boss.bullets[i].vx = -8.0f;
					boss.bullets[i].vy = angles[shot] * 8.0f;
					boss.bullets[i].active = true;
					shot++;
				}
			}
		}
		else {
			// 模式2：追踪弹（发射时指向玩家）
			float dx = (float)(h->locx - boss.locx);
			float dy = (float)(h->locy - boss.locy);
			float len = sqrtf(dx * dx + dy * dy);
			if (len < 1.0f) len = 1.0f;
			for (int i = 0; i < MAX_BULLET; i++) {
				if (!boss.bullets[i].active) {
					boss.bullets[i].x = (float)boss.locx;
					boss.bullets[i].y = (float)(boss.locy + 40);
					boss.bullets[i].vx = dx / len * 10.0f;
					boss.bullets[i].vy = dy / len * 10.0f;
					boss.bullets[i].active = true;
					break;
				}
			}
		}
	}

	// ---- 移动弹幕（加入vy）----
	for (int i = 0; i < MAX_BULLET; i++) {
		if (!boss.bullets[i].active) continue;
		boss.bullets[i].x += boss.bullets[i].vx;
		boss.bullets[i].y += boss.bullets[i].vy;   // 新增Y轴移动
		if (boss.bullets[i].x < -50 || boss.bullets[i].y < -50
			|| boss.bullets[i].y > 800) {
			boss.bullets[i].active = false;
		}

		// ---- 碰撞检测：弹幕打中玩家 ----
		// 用角色中心矩形做判断（图片384x384，取中心区域避免误判）
		int hx = h->locx + 10, hy = h->locy + 5;   // 玩家碰撞箱左上角
		int hw = 60, hh = 70;              // 碰撞箱宽高（缩小避免误判）
		int bx = (int)boss.bullets[i].x;
		int by = (int)boss.bullets[i].y;

		if (bx > hx && bx < hx + hw && by > hy && by < hy + hh) {
			int actualdamage = boss.damage;
			if (h->iscrouching) actualdamage = boss.damage / 2;
			h->lefthp -= actualdamage;
			h->hitFlashTimer = 12;
			if (h->lefthp < 0) h->lefthp = 0;
			boss.bullets[i].active = false;           // 击中后弹幕消失
			if (h->lefthp == 0) h->survive = false;
		}
	}
	handleAttack(&qy);
}

void drawBoss(IMAGE* bossImg)
{
	if (!boss.survive) return;

	// 绘制 Boss 图片
	putimage(boss.locx, boss.locy, bossImg);

	// 绘制弹幕（暂用红色圆圈代替，后续可换图片）
	for (int i = 0; i < MAX_BULLET; i++) {
		if (!boss.bullets[i].active) continue;
		setfillcolor(RED);
		solidcircle((int)boss.bullets[i].x, (int)boss.bullets[i].y, 15);
	}
}

void drawHUD(hero* h)
{
	// -------- 玩家血条（左上角）--------
	setfillcolor(RGB(60, 60, 60));
	solidrectangle(30, 30, 230, 55);         // 背景
	int playerBarW = 200 * h->lefthp / h->totalhp;
	setfillcolor(RGB(80, 200, 80));
	solidrectangle(30, 30, 30 + playerBarW, 55);                       // 绿色血量
	setbkmode(TRANSPARENT);
	settextcolor(WHITE);
	settextstyle(18, 0, _T("黑体"));
	outtextxy(35, 58, _T("玩家 HP"));

	// -------- Boss 血条（右上角）--------
	setfillcolor(RGB(60, 60, 60));
	solidrectangle(1050, 30, 1250, 55);
	int bossBarW = 200 * boss.lefthp / boss.totalhp;
	// 阶段不同颜色不同：绿→黄→红
	COLORREF bossColor = RGB(80, 200, 80);
	if (boss.phase == 1) bossColor = RGB(240, 180, 0);
	if (boss.phase == 2) bossColor = RGB(220, 50, 50);
	setfillcolor(bossColor);
	solidrectangle(1050, 30, 1050 + bossBarW, 55);
	settextcolor(WHITE);
	outtextxy(1055, 58, _T("BOSS HP"));

	// -------- Boss 阶段提示 --------
	if (boss.phase == 1) {
		settextcolor(RGB(240, 180, 0));
		settextstyle(22, 0, _T("黑体"));
		outtextxy(560, 20, _T("Boss 进入狂暴状态！"));
	}
	if (boss.phase == 2) {
		settextcolor(RGB(220, 50, 50));
		settextstyle(22, 0, _T("黑体"));
		outtextxy(560, 20, _T("Boss 濒死爆发！"));
	}

	// -------- 技能冷却显示（屏幕下方中央）--------
	SkillData* sd = roles ? xjSkills : qySkills;
	hero* h1 = &qy;
	for (int i = 0; i < 3; i++) {
		int bx = 490 + i * 100;  // 三个格子横排
		int by = 700;

		// 背景格
		setfillcolor(RGB(40, 40, 40));
		solidrectangle(bx, by, bx + 80, by + 40);

		// 冷却遮罩
		if (h1->skillTimer[i] > 0) {
			setfillcolor(RGB(20, 20, 20));
			int maskH = 40 * h1->skillTimer[i] / sd[i].cooldown;
			solidrectangle(bx, by, bx + 80, by + maskH);
		}

		// 技能编号文字
		setbkmode(TRANSPARENT);
		settextcolor(h1->skillTimer[i] > 0 ? DARKGRAY : WHITE);
		settextstyle(20, 0, _T("黑体"));
		TCHAR buf[4];
		_stprintf_s(buf, _T("%d"), i + 1);
		outtextxy(bx + 30, by + 10, buf);
	}

	// -------- 游戏计时（屏幕顶部中央）--------
	int sec = gameFrames / 100;   // Sleep(10)约100帧/秒
	int minutes = sec / 60;
	int seconds = sec % 60;
	TCHAR timeBuf[20];
	_stprintf_s(timeBuf, _T("%02d:%02d"), minutes, seconds);
	settextcolor(WHITE);
	settextstyle(28, 0, _T("黑体"));
	setbkmode(TRANSPARENT);
	outtextxy(610, 15, timeBuf);
}

// ============================================
// showWinScreen() - 胜利界面
// 显示得分，按R重新开始，按ESC退出
// ============================================
void showWinScreen(hero* h)
{
	// 半透明遮罩（深绿色）
	setfillcolor(RGB(0, 60, 0));
	setfillstyle(BS_SOLID);
	BLENDFUNCTION bf = { 0 };
	// EasyX没有原生半透明，用深色矩形模拟
	solidrectangle(300, 200, 980, 580);

	// 标题
	settextstyle(60, 0, _T("黑体"));
	settextcolor(RGB(255, 215, 0));  // 金色
	setbkmode(TRANSPARENT);
	outtextxy(430, 220, _T("胜利！"));

	// 得分
	TCHAR scoreBuf[40];
	_stprintf_s(scoreBuf, _T("最终得分：%d"), finalScore);
	settextstyle(36, 0, _T("黑体"));
	settextcolor(WHITE);
	outtextxy(420, 320, scoreBuf);

	// 剩余血量和用时
	int sec = gameFrames / 100;
	TCHAR detailBuf[60];
	_stprintf_s(detailBuf, _T("剩余血量：%d    用时：%d秒"), h->lefthp, sec);
	settextstyle(24, 0, _T("黑体"));
	settextcolor(RGB(180, 255, 180));
	outtextxy(370, 390, detailBuf);

	// 提示
	settextstyle(22, 0, _T("黑体"));
	settextcolor(RGB(200, 200, 200));
	outtextxy(390, 480, _T("按 R 保存成绩并返回菜单"));
	outtextxy(390, 515, _T("按 ESC 直接退出"));
}

// ============================================
// showLoseScreen() - 失败界面
// ============================================
void showLoseScreen()
{
	solidrectangle(300, 200, 980, 550);  // 深色背景复用，颜色不同
	setfillcolor(RGB(60, 0, 0));
	solidrectangle(300, 200, 980, 550);

	settextstyle(60, 0, _T("黑体"));
	settextcolor(RGB(220, 50, 50));
	setbkmode(TRANSPARENT);
	outtextxy(410, 220, _T("失败..."));

	int sec = gameFrames / 100;
	TCHAR detailBuf[40];
	_stprintf_s(detailBuf, _T("坚持了 %d 秒"), sec);
	settextstyle(30, 0, _T("黑体"));
	settextcolor(WHITE);
	outtextxy(490, 330, detailBuf);

	settextstyle(22, 0, _T("黑体"));
	settextcolor(RGB(200, 200, 200));
	outtextxy(390, 430, _T("按 R 重新开始"));
	outtextxy(390, 465, _T("按 ESC 退出"));
}

void move(hero* h)
{
	if (h->castTimer > 0) return;

	static bool lastW = false;
	bool keyW = (GetAsyncKeyState('W') & 0x8000) != 0;
	bool keyA = (GetAsyncKeyState('A') & 0x8000) != 0;
	bool keyS = (GetAsyncKeyState('S') & 0x8000) != 0;
	bool keyD = (GetAsyncKeyState('D') & 0x8000) != 0;
	bool keyF = (GetAsyncKeyState('F') & 0x8000) != 0;

	// 根据按键更新坐标
	if (keyA) {
		h->locx -= 5; lastad = 0;
		if (h->locx < LEFT_BOUND) h->locx = LEFT_BOUND;
	}
	if (keyD) {
		h->locx += 5; lastad = 1;
		if (h->locx > RIGHT_BOUND) h->locx = RIGHT_BOUND;
	}
	if (keyF) {
		h->locx += 10; lastad = 1;
		if (h->locx > RIGHT_BOUND) h->locx = RIGHT_BOUND;
	}
	//二段跳
	bool wPressed = keyW && !lastW;
	if (wPressed && h->jumpcount < 2) {
		h->velY = JUMP_POWER;   // 给一个向上的速度
		h->jumpcount++;
		h->onground = false;
		h->iscrouching = false; // 跳跃时取消下蹲
	}
	lastW = keyW;
	if (keyS && h->onground) {
		h->iscrouching = true;
	}
	else if (!keyS) {
		h->iscrouching = false;
	}
}

void applyPhysics(hero* h)//重力系统
{
	if (!h->onground) {
		h->velY += GRAVITY;        // 重力加速度累加
		h->locy += (int)h->velY;   // 用速度更新Y坐标
	}

	// --- 落地检测 ---
	if (h->locy >= GROUND_Y) {
		h->locy = GROUND_Y;        // 贴地，防止穿模
		h->velY = 0;
		h->jumpcount = 0;          // 落地后重置跳跃次数
		h->onground = true;
	}
}

IMAGE* getHeroFrame(hero* h)
{
	herostates * sta = roles ? &xjsta : &qysta;
	int dir = lastad ? 1 : 0;
	if (h->iscrouching) return &sta->crouch[dir];
	if (!h->onground)   return &sta->jump[dir];
	return &sta->stand[dir];
}


void itg() //inside the game
{
	if (isstart)                                                   ////////////////////////////////////
	{                                                              //命名方式：人物 状态 朝向
		initgraph(1280, 750);                                      //奇犽：a 小杰 ：b
		IMAGE background;                                          //直立：a 跳跃：b 蹲下：c
		IMAGE bossImg;														   //左：a 右 b
		loadimage(&bossImg, _T("xj_left_resized.jpg"));
		loadimage(&qysta.stand[0], _T("qy_left_resized.jpg"));
		loadimage(&qysta.stand[1], _T("qy_right_resized.jpg"));
		loadimage(&qysta.jump[0], _T("qy_jump_left_resized.jpg"));
		loadimage(&qysta.jump[1], _T("qy_jump_right_resized.jpg"));
		loadimage(&qysta.crouch[0], _T("qy_xd_left_resized.jpg"));
		loadimage(&qysta.crouch[1], _T("qy_xd_right_resized.jpg"));
		loadimage(&xjsta.stand[0], _T("xj_left_resized.jpg"));
		loadimage(&xjsta.stand[1], _T("xj_right_resized.jpg"));
		loadimage(&xjsta.jump[0], _T("xj_jump_left_resized.jpg"));
		loadimage(&xjsta.jump[1], _T("xj_jump_right_resized.jpg"));
		loadimage(&xjsta.crouch[0], _T("xj_xd_left_resized.jpg"));
		loadimage(&xjsta.crouch[1], _T("xj_xd_right_resized.jpg"));
		loadimage(&background, _T("game_background.jpg"));
		initgame();
		gameFrames = 0;
		gameLose = false;
		gameWin = false;
		while (!GetAsyncKeyState(27))
		{
			BeginBatchDraw();
			cleardevice();
			putimage(0, 0, &background);
			if (qy.hitFlashTimer > 0) {
				qy.hitFlashTimer--;
				// 每2帧交替显示，产生闪烁感
				if (qy.hitFlashTimer % 2 == 0) {
					setfillcolor(RGB(255, 0, 0));
					setfillstyle(BS_SOLID);
					// 在角色位置画半透明红框（EasyX用透明色模拟）
					rectangle(qy.locx + 10, qy.locy + 5, qy.locx + 70, qy.locy + 75);
				}
				// 显示浮动伤害数字
				TCHAR dmgBuf[8];
				int actualdamage = boss.damage;
				if (qy.iscrouching) actualdamage /= 2;
				_stprintf_s(dmgBuf, _T("-%d"), actualdamage);
				settextstyle(22, 0, _T("黑体"));
				settextcolor(RGB(255, 80, 80));
				setbkmode(TRANSPARENT);
				outtextxy(qy.locx + 20, qy.locy - 10 - (12 - qy.hitFlashTimer) * 2, dmgBuf);
			}
			//tool();
			if (!gameWin && !gameLose) {
				gameFrames++;//计算时间
				move(&qy);
				applyPhysics(&qy);
				bossAI(&qy);//bossai里面有handleattack
				IMAGE* t = getHeroFrame(&qy);
				putimage(qy.locx, qy.locy, t);
				drawBoss(&bossImg);
				drawEffects();
				drawHUD(&qy);
				int result = checkGameOver(&qy);
			}
			// ---- 结算界面 ----
			if (gameWin) {
				drawBoss(&bossImg);
				showWinScreen(&qy);
				EndBatchDraw(); // 先结束当前帧再弹输入框

				if (GetAsyncKeyState('R') & 0x8000) {
					Sleep(200); // 防止R键残留
					char nickname[22] = { 0 };
					inputNickname(nickname);  // 独立运行，不嵌套在BatchDraw里
					saveRank(finalScore, nickname);

					// 存档成功提示
					BeginBatchDraw();
					setfillcolor(RGB(0, 100, 0));
					solidrectangle(400, 340, 880, 390);
					settextstyle(22, 0, _T("黑体"));
					settextcolor(RGB(150, 255, 150));
					setbkmode(TRANSPARENT);
					TCHAR tip[48];
					_stprintf_s(tip, _T("已保存：%hs  得分：%d"), nickname, finalScore);
					outtextxy(415, 353, tip);
					EndBatchDraw();
					Sleep(1500); // 显示1.5秒后退出
					break;
				}
				BeginBatchDraw(); // 没按R就继续下一帧
			}

			if (gameLose) {
				showLoseScreen();
				if (GetAsyncKeyState('R') & 0x8000) {
					// 重新开始：重置所有状态
					initgame();
					gameFrames = 0;
					gameWin = false;
					gameLose = false;
				}
			}
			Sleep(10);
			EndBatchDraw();
		}
		printf("gaming over!");
	}
	return;
}
int main()
{
	initmenu();
	itg();
	return 0;
}

