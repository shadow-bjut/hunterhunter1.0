#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "winmm.lib")          // ← 新增：多媒体库
#define _CRT_SECURE_NO_WARNINGS
#include<graphics.h>
#include<conio.h>
#include<Windows.h>
#include <mmsystem.h> 
#include<stdio.h>
#include<math.h>
#define RANK_FILE "scores.dat"
#define RANK_SIZE 5
const int FPS = 100;   // Sleep(10) ≈ 100帧/秒
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
bool mute = false;


// ============================================================
// 背景音乐控制（基于 MCI，支持 MP3）
// 用法：
//   playBGM("menu.mp3");   // 播放并循环
//   stopBGM();             // 停止
// ============================================================
static char g_bgmAlias[32] = "";   // 当前已打开的别名

void stopBGM()
{
	if (g_bgmAlias[0] == '\0') return;
	mciSendStringA("stop bgm", NULL, 0, NULL);
	mciSendStringA("close bgm", NULL, 0, NULL);
	g_bgmAlias[0] = '\0';
}

// 播放一首新 BGM（自动停止上一首）
// repeat=true 时无限循环
void playBGM(const char* filename, bool repeat = true)
{
	stopBGM();

	char cmd[256];
	sprintf_s(cmd, "open \"%s\" type mpegvideo alias bgm", filename);
	if (mciSendStringA(cmd, NULL, 0, NULL) != 0) return;  // 文件不存在则跳过

	strcpy_s(g_bgmAlias, "bgm");

	if (repeat)
		mciSendStringA("play bgm repeat", NULL, 0, NULL);
	else
		mciSendStringA("play bgm", NULL, 0, NULL);
}

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

		// ---- WM_KEYDOWN + GetKeyState 判断字符 ----
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
		if (tx >= 100 && tx <= 350 && ty >= 719 && ty <= 780) mute = true;
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
	playBGM("menu1.mp3");
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
		outtextxy(100, 719, _T("点击此处静音"));
		if (mute) settextcolor(RED);
		if (mute) stopBGM();
		Sleep(10);
		EndBatchDraw();
		if(isstart) break;
	}
	if(!mute) stopBGM();
	closegraph();
}

//有关游戏主体的有关函数：

struct SkillData {
	int castFrames;    // 前摇帧数（期间不能移动）
	int damage;        // 命中伤害
	int cooldown;      // 冷却帧数
};

SkillData qySkills[3] = {
	{20, 25, 420},   // 技能1：前摇短、伤害中、冷却3秒
	{40, 45, 300},   // 技能2：
	{10, 15, 120},   // 技能3：前摇很短、伤害低、冷却2秒
};

// 小杰三个技能
SkillData xjSkills[3] = {
	{15, 150, 150},   // 技能1
	{50, 40, 360},   // 技能2
	{25, 20, 200},   // 技能3
};

//特效

struct Effect {
	int x, y;
	int timer;      // 剩余显示帧数
	int type;       // 0=普攻 1=技能1 2=技能2 3=技能3
	int herorole; //0 奇犽 1 小杰
	bool active;
};
const int MAX_EFFECT = 8;
Effect effects[MAX_EFFECT];

void spawnEffect(int x, int y, int type,int role)
{
	for (int i = 0; i < MAX_EFFECT; i++) {
		if (!effects[i].active) {
			effects[i] = { x, y, 18, type,role, true };
			return;
		}
	}
}


// PNG透明绘制：读取Alpha通道，alpha=0的像素跳过
// 品红色(R=255,G=0,B=255)作为透明键，跳过不绘制
// EasyX缓冲格式：0x00BBGGRR，所以品红存储值为 0x00FF00FF
void putImageTransparent(int dstX, int dstY, IMAGE* img)
{
	HDC dstDC = GetImageHDC(NULL);      // 当前绘图缓冲（BeginBatchDraw期间有效）
	HDC srcDC = GetImageHDC(img);       // 图片自己的HDC

	TransparentBlt(
		dstDC,                          // 目标DC
		dstX, dstY,                     // 目标位置
		img->getwidth(),                // 目标宽
		img->getheight(),               // 目标高
		srcDC,                          // 源DC
		0, 0,                           // 源起点
		img->getwidth(),                // 源宽
		img->getheight(),               // 源高
		RGB(255, 0, 255)                // 品红 = 透明色
	);
}

void drawEffects()
{
	for (int i = 0; i < MAX_EFFECT; i++) {
		if (!effects[i].active) continue;

		int r = effects[i].timer * 3;   // 扩散半径
		int cx = effects[i].x;
		int cy = effects[i].y;
		int t = effects[i].timer;

		// ===================== 奇犽（电气系）=====================
		if (effects[i].herorole == 0) {
			switch (effects[i].type) {

			case 0: // 普攻：白色闪电短线放射
				setcolor(RGB(200, 230, 255));
				for (int j = 0; j < 5; j++) {
					float ang = j * 3.14159f * 2 / 5 + t * 0.3f;
					line(cx, cy,
						cx + (int)(cosf(ang) * r * 1.5f),
						cy + (int)(sinf(ang) * r * 1.5f));
				}
				break;

			case 1: // 技能1：蓝色电弧双环 + 内部闪光
				setcolor(RGB(100, 200, 255));
				circle(cx, cy, r);
				circle(cx, cy, r * 2);
				setcolor(RGB(255, 255, 255));
				circle(cx, cy, r / 2);
				break;

			case 2: // 技能2（大招）：蓝白放射爆炸
				setfillcolor(RGB(180, 220, 255));
				solidcircle(cx, cy, r * 2);
				setfillcolor(RGB(255, 255, 255));
				solidcircle(cx, cy, r);
				// 闪电射线
				setcolor(RGB(0, 180, 255));
				for (int j = 0; j < 8; j++) {
					float ang = j * 3.14159f / 4;
					line(cx, cy,
						cx + (int)(cosf(ang) * r * 3),
						cy + (int)(sinf(ang) * r * 3));
				}
				break;

			case 3: // 技能3：蓝色球状闪电波
				// 外层漫射光晕
				setfillcolor(RGB(0, 60, 180));
				solidcircle(cx, cy, r * 3);
				// 中层亮蓝球体
				setfillcolor(RGB(40, 140, 255));
				solidcircle(cx, cy, r * 2);
				// 内核白色核心
				setfillcolor(RGB(200, 230, 255));
				solidcircle(cx, cy, r);
				// 旋转闪电射线（随timer旋转）
				setcolor(RGB(150, 210, 255));
				for (int j = 0; j < 6; j++) {
					float ang = j * 3.14159f / 3.0f + t * 0.25f;  // 旋转
					int ex = cx + (int)(cosf(ang) * r * 3.5f);
					int ey = cy + (int)(sinf(ang) * r * 3.5f);
					// 折线模拟闪电锯齿
					int mx = cx + (int)(cosf(ang) * r * 1.8f);
					int my = cy + (int)(sinf(ang) * r * 1.8f);
					line(cx, cy, mx + (int)(sinf(ang) * r * 0.5f),
						my - (int)(cosf(ang) * r * 0.5f));
					line(mx + (int)(sinf(ang) * r * 0.5f),
						my - (int)(cosf(ang) * r * 0.5f), ex, ey);
				}
				break;
			}
		}

		// ===================== 小杰（自然能量系）=====================
		else {
			switch (effects[i].type) {

			case 0: // 普攻：绿色同心扩散圆
				setcolor(RGB(80, 220, 80));
				circle(cx, cy, r);
				setcolor(RGB(200, 255, 100));
				circle(cx, cy, r / 2);
				break;

			case 1: // 技能1：黄绿扇形冲击波
				setcolor(RGB(180, 255, 60));
				for (int j = -3; j <= 3; j++) {
					float ang = 3.14159f + j * 0.15f; // 朝左发射
					line(cx, cy,
						cx + (int)(cosf(ang) * r * 2.5f),
						cy + (int)(sinf(ang) * r * 2.5f));
				}
				setcolor(RGB(255, 230, 0));
				circle(cx, cy, r);
				break;

			case 2: // 技能2（大招）：金橙大爆炸（千手棒）
				setfillcolor(RGB(255, 160, 0));
				solidcircle(cx, cy, r * 2);
				setfillcolor(RGB(255, 255, 100));
				solidcircle(cx, cy, r);
				// 四向冲击棒
				setcolor(RGB(255, 100, 0));
				for (int j = 0; j < 4; j++) {
					float ang = j * 3.14159f / 2;
					// 粗线模拟棒状
					for (int off = -3; off <= 3; off++) {
						line(cx + off, cy,
							cx + (int)(cosf(ang) * r * 3) + off,
							cy + (int)(sinf(ang) * r * 3));
					}
				}
				break;

			case 3: // 技能3：橙色能量环脉冲
				setcolor(RGB(255, 140, 0));
				circle(cx, cy, r * 2);
				setfillcolor(RGB(255, 200, 50));
				solidcircle(cx, cy, r);
				setcolor(RGB(255, 255, 200));
				circle(cx, cy, r * 3);
				break;
			}
		}

		// ---- 每帧衰减 ----
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
	int invincibleTimer; //无敌帧

	//----奇犽特有技能---//
	int speedBoostTimer;   // 技能1加速剩余帧数
	bool isDashing;        // 技能2突刺中
	int dashTimer;         // 突刺剩余帧数
	//----小杰---------//
	int  xjChargeTimer;   // 技能1蓄力帧数
	bool xjIsCharging;    // 是否正在蓄力
	int  xjSwordTimer;    // 技能2光剑动画计时
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

struct HeroBullet {
	float x, y;
	float vx;
	bool  active;
	int   damage;
};
const int MAX_HERO_BULLET = 5;
HeroBullet heroBullets[MAX_HERO_BULLET];

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
	float bossVelX;     // 水平移动速度
	int   moveTimer;    // 移动方向计时器
}boss;


// 得分公式：基础分 + 血量奖励 - 时间惩罚
// 基础分1000，每1hp加10分，每秒超过30秒扣5分
int calcScore(hero* h)
{
	int timeSeconds = gameFrames / FPS;
	int score = 1000;
	score += h->lefthp * 10;                  // 血量奖励
	int overTime = max(0, timeSeconds - 30);  // 超过30秒开始扣分
	score -= overTime * 5;
	if (score < 0) score = 0;
	switch (difficulties) {         //难度系数
	case -1:
		break;
	case 0:
		score *= 2;
		break;
	case 1:
		score *= 3;
		break;
	}
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

// ----普攻结算（奇犽和小杰共用）----
void resolveNormalAttack(hero* h)
{
	int dist = boss.locx - h->locx;
	spawnEffect(h->locx + 80, h->locy + 40, 0, roles);
	if (dist > 0 && dist < 150 && boss.survive) {
		boss.lefthp -= 8;
		if (boss.lefthp < 0) boss.lefthp = 0;
		if (boss.lefthp == 0) boss.survive = false;
	}
	h->currentSkill = -1;
	h->isAttacking = false;
}

// ---- J键普攻输入检测（奇犽和小杰共用）----
bool handleNormalAttackInput(hero* h)
{
	bool keyJ = (GetAsyncKeyState('J') & 0x8000) != 0;
	if (keyJ && h->attackTimer == 0) {
		h->attackTimer = 30;
		h->castTimer = 10;
		h->currentSkill = -2;
		h->isAttacking = true;
		return true;
	}
	return false;
}

void handleAttack(hero* h)
{
	// ---- 所有计时器每帧-1 ----
	if (h->invincibleTimer > 0) h->invincibleTimer--;
	if (h->attackTimer > 0) h->attackTimer--;
	if (h->castTimer > 0) h->castTimer--;
	for (int i = 0; i < 3; i++)
		if (h->skillTimer[i] > 0) h->skillTimer[i]--;


	// ============ 小杰专属技能（完全独立处理后 return）============
	if (roles == 1) {
		bool key1 = (GetAsyncKeyState('1') & 0x8000) != 0;
		bool key2 = (GetAsyncKeyState('2') & 0x8000) != 0;
		bool key3 = (GetAsyncKeyState('3') & 0x8000) != 0;

		// ---- 技能1：蓄力光球（按住蓄力，松手释放）----
		if (h->skillTimer[0] == 0) {
			if (key1 && !h->xjIsCharging) {
				h->xjIsCharging = true;   // 开始蓄力
			}
			if (h->xjIsCharging && key1) {
				h->xjChargeTimer++;
				if (h->xjChargeTimer > 120) h->xjChargeTimer = 120; // 最多蓄2秒
			}
			if (h->xjIsCharging && !key1) {
				// 蓄力伤害：最低10，蓄满约40，不超过Boss总血量的25%
				int chargeDmg = 10 + h->xjChargeTimer / 3;      
				int dmgCap = boss.totalhp / 4;                   //25%上限
				if (chargeDmg > dmgCap) chargeDmg = dmgCap;

				// 自我损耗：只扣技能伤害的20%
				int selfDmg = chargeDmg / 5;                     
				if (h->invincibleTimer == 0) {                  
					h->lefthp -= selfDmg;
					h->hitFlashTimer = 12;
					h->invincibleTimer = 30;                     // ← 释放后给短暂无敌
					if (h->lefthp <= 0) h->survive = false;
				}

				int dist = boss.locx - h->locx;
				if (dist > 0 && dist < 250 && boss.survive) {
					boss.lefthp -= chargeDmg;
					if (boss.lefthp < 0) boss.lefthp = 0;
					if (boss.lefthp == 0) boss.survive = false;
				}
				spawnEffect(h->locx + 110, h->locy + 40, 1, 1);
				h->skillTimer[0] = xjSkills[0].cooldown;
				h->xjChargeTimer = 0;
				h->xjIsCharging = false;
			}
		}

		// ---- 技能2：光剑挥动 ----
		if (key2 && h->skillTimer[1] == 0 && h->castTimer == 0) {
			h->xjSwordTimer = 22;          // 动画22帧
			h->castTimer = 18;          // 前18帧锁定移动
			h->skillTimer[1] = xjSkills[1].cooldown;
		}
		if (h->xjSwordTimer > 0) {
			h->xjSwordTimer--;
			if (h->xjSwordTimer == 11) {    // 挥到中段时结算伤害
				int dist = boss.locx - h->locx;
				if (dist > 0 && dist < 200 && boss.survive) {
					boss.lefthp -= xjSkills[1].damage;
					if (boss.lefthp < 0) boss.lefthp = 0;
					if (boss.lefthp == 0) boss.survive = false;
				}
				spawnEffect(h->locx + 150, h->locy + 30, 2, 1);
			}
		}

		// ---- 技能3：发射小光球 ----
		if (key3 && h->skillTimer[2] == 0) {
			h->skillTimer[2] = xjSkills[2].cooldown;
			for (int i = 0; i < MAX_HERO_BULLET; i++) {
				if (!heroBullets[i].active) {
					heroBullets[i] = {
						(float)(h->locx + 90),
						(float)(h->locy + 45),
						14.0f, true,
						xjSkills[2].damage
					};
					break;
				}
			}
		}
		// ---- 前摇结束时结算 ----
		if (h->currentSkill == -2) {          // 普攻前摇结束
			resolveNormalAttack(h); return;
		}
		if (handleNormalAttackInput(h)) return;
		return; // xj处理完毕，跳过奇犽通用逻辑
	}
	// ============ 小杰专属结束 ============
	
	// 前摇期间不处理新输入
	if (h->castTimer > 0) return;

	// ---- 前摇结束时结算 ----
	if (h->currentSkill >= 0) {
		SkillData* sd = roles ? xjSkills : qySkills;

		if (!roles && h->currentSkill == 0) {
			// 奇犽技能1：加速Buff，不造成伤害
			h->speedBoostTimer = 180;  // 约3秒
			spawnEffect(h->locx + 40, h->locy + 40, 1, 0); // 蓝色光环提示
		}
		else if (!roles && h->currentSkill == 1) {
			// 奇犽技能2：启动突刺，伤害在move()里碰撞时结算
			h->isDashing = true;
			h->dashTimer = 36;        // 突刺持续18帧
		}
		else {
			// 其余技能（奇犽技能3 / 小杰全部）：正常伤害判定
			int dist = boss.locx - h->locx;
			if (dist > 0 && dist < 300) {
				boss.lefthp -= sd[h->currentSkill].damage;
				if (boss.lefthp < 0) boss.lefthp = 0;
				if (boss.lefthp == 0) boss.survive = false;
			}
		}
		h->currentSkill = -1;
	}
	// ---- 前摇结束时结算 ----
	if (h->currentSkill == -2) { resolveNormalAttack(h); return; } 
	if (handleNormalAttackInput(h)) return;                         

	// ---- 1/2/3技能 ----
	SkillData* sd = roles ? xjSkills : qySkills;
	for (int i = 0; i < 3; i++) {
		bool keyPressed = (GetAsyncKeyState('1' + i) & 0x8000) != 0;
		if (keyPressed && h->skillTimer[i] == 0) {
			h->castTimer = sd[i].castFrames;
			h->skillTimer[i] = sd[i].cooldown;
			h->currentSkill = i;
			spawnEffect(h->locx + 80, h->locy + 40, h->currentSkill + 1,roles);
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
	qy.speedBoostTimer = 0;
	qy.isDashing = false;
	qy.dashTimer = 0;
	xj.speedBoostTimer = 0;
	xj.isDashing = false;
	xj.dashTimer = 0;

	xj.xjChargeTimer = 0;
	xj.xjIsCharging = false;
	xj.xjSwordTimer = 0;
	for (int i = 0; i < MAX_HERO_BULLET; i++) heroBullets[i].active = false;

	qy.invincibleTimer = 0;
	xj.invincibleTimer = 0;
}

void initboss()
{
	boss.locx = 900;           // Boss 固定在右侧
	boss.locy = GROUND_Y-30;
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

	boss.bossVelX = 0;
	boss.moveTimer = 0;
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
	// ---- Boss 移动逻辑（阶段1以上才开始走动）----
	if (boss.phase >= 1) {
		boss.moveTimer--;
		if (boss.moveTimer <= 0) {
			// 每隔一段时间重新决定方向
			int dist =abs(h->locx - boss.locx) ;
			if (dist > 200) {
				boss.bossVelX = -2.0f;          // 玩家离太远，向左靠近
			}
			else if (dist < 80) {
				boss.bossVelX = 2.0f;           // 玩家太近，向右躲避
			}
			else {
				boss.bossVelX = 0;              // 距离合适，原地
			}
			boss.moveTimer = 40;
		}
		boss.locx += (int)boss.bossVelX;

		// Boss 活动范围限制（右侧700~1000之间）
		if (boss.locx < 700)  boss.locx = 700;
		if (boss.locx > 1000) boss.locx = 1000;
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

			if (h->invincibleTimer > 0) {          // ← 新增：无敌期间跳过
				boss.bullets[i].active = false;    //   但子弹仍然消失（不穿透）
				continue;
			}

			int actualdamage = boss.damage;
			if (h->iscrouching) actualdamage = boss.damage / 2;
			h->lefthp -= actualdamage;
			h->hitFlashTimer = 12;
			h->invincibleTimer = 60;               // ← 新增：被打后给60帧（约0.6秒）无敌
			if (h->lefthp < 0) h->lefthp = 0;
			boss.bullets[i].active = false;
			if (h->lefthp == 0) h->survive = false;
		}
	}
	handleAttack(h);
}

// ---- 小杰技能1：蓄力光球实时绘制 ----
void drawXJCharge(hero* h)
{
	if (!h->xjIsCharging) return;
	int radius = 10 + h->xjChargeTimer / 4;
	if (radius > 42) radius = 42;
	int cx = h->locx + 110, cy = h->locy + 40;

	// 外层光晕（随蓄力变大）
	setfillcolor(RGB(200, 140, 0));
	solidcircle(cx, cy, radius + 6);
	// 主球体
	setfillcolor(RGB(255, 210, 20));
	solidcircle(cx, cy, radius);
	// 内核
	setfillcolor(RGB(255, 255, 180));
	solidcircle(cx, cy, radius / 2);
	// 满蓄力（>60帧）外圈白色闪烁
	if (h->xjChargeTimer > 60 && (gameFrames % 6) < 3) {
		setcolor(RGB(255, 255, 255));
		circle(cx, cy, radius + 10);
	}
}

// ---- 小杰技能2：光剑挥动绘制 ----
void drawXJSword(hero* h)
{
	if (h->xjSwordTimer <= 0) return;
	// progress 0.0（刚开始）→ 1.0（结束）
	float progress = 1.0f - (float)h->xjSwordTimer / 22.0f;
	float startAng = -0.75f;          // 右上方开始
	float ang = startAng + progress * 1.5f; // 向右下挥
	int sx = h->locx + 65, sy = h->locy + 35;

	// 剑身（黄色粗线）
	int ex = sx + (int)(cosf(ang) * 130);
	int ey = sy + (int)(sinf(ang) * 130);
	setcolor(RGB(255, 230, 60));
	for (int off = -4; off <= 4; off++)
		line(sx + off, sy, ex + off, ey);
	// 剑光内芯
	setcolor(RGB(255, 255, 220));
	for (int off = -1; off <= 1; off++)
		line(sx + off, sy, ex + off, ey);
	// 挥动轨迹弧（5条渐隐线）
	for (int j = 1; j <= 5; j++) {
		float trailAng = ang - j * 0.13f;
		int trailLen = 130 - j * 15;
		int tx2 = sx + (int)(cosf(trailAng) * trailLen);
		int ty2 = sy + (int)(sinf(trailAng) * trailLen);
		setcolor(RGB(255, 200 - j * 20, 0));
		line(sx, sy, tx2, ty2);
	}
}

// ---- 小杰技能3：光球飞行更新 + 绘制 ----
void updateAndDrawHeroBullets()
{
	for (int i = 0; i < MAX_HERO_BULLET; i++) {
		if (!heroBullets[i].active) continue;
		heroBullets[i].x += heroBullets[i].vx;

		int cx = (int)heroBullets[i].x;
		int cy = (int)heroBullets[i].y;

		// 外层光晕
		setfillcolor(RGB(255, 180, 0));
		solidcircle(cx, cy, 16);
		// 主球
		setfillcolor(RGB(255, 240, 80));
		solidcircle(cx, cy, 11);
		// 内核
		setfillcolor(RGB(255, 255, 210));
		solidcircle(cx, cy, 5);
		// 飞行轨迹（每帧在身后留3个渐隐点）
		for (int j = 1; j <= 3; j++) {
			setfillcolor(RGB(255, 200 - j * 30, 0));
			solidcircle(cx - j * 14, cy, 8 - j * 2);
		}

		// 出界
		if (heroBullets[i].x > 1300) {
			heroBullets[i].active = false;
			continue;
		}
		// 碰撞Boss
		if (cx > boss.locx && cx < boss.locx + 120 &&
			cy > boss.locy && cy < boss.locy + 120 && boss.survive) {
			boss.lefthp -= heroBullets[i].damage;
			if (boss.lefthp < 0) boss.lefthp = 0;
			if (boss.lefthp == 0) boss.survive = false;
			spawnEffect(cx, cy, 3, 1);           // 命中特效
			heroBullets[i].active = false;
		}
	}
}

void drawBoss(IMAGE* bossImg)
{
	if (!boss.survive) return;

	// 绘制 Boss 图片
	putImageTransparent(boss.locx, boss.locy, bossImg);

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
	if(1.0*h->lefthp / h->totalhp>=0.5) setfillcolor(RGB(80, 200, 80));
	else if (1.0 * h->lefthp / h->totalhp >= 0.2) setfillcolor(YELLOW);
	else setfillcolor(RED);
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
	hero* h1 = roles ? &xj : &qy;   // ← 跟随当前角色
	for (int i = 0; i < 3; i++) {
		int bx = 510 + i * 100;  // 三个格子横排
		int by = 50;

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
	// ======== 突刺逻辑（最高优先级，castTimer不拦截）========
	if (h->isDashing) {
		int dashDir = lastad ? 1 : -1;        // ← 按起手方向决定
		h->locx += 22 * dashDir;              // 每帧冲22px
		h->dashTimer--;

		// 边界限制
		if (h->locx < LEFT_BOUND)  h->locx = LEFT_BOUND;
		if (h->locx > RIGHT_BOUND) h->locx = RIGHT_BOUND;

		// 碰撞判定
		int heroCenter = h->locx + 50;
		int dist = abs(boss.locx + 60 - heroCenter);
		if (dist < 120 && boss.survive) {
			// 突刺只打一次，打中后立即终止
			boss.lefthp -= qySkills[1].damage;
			if (boss.lefthp < 0) boss.lefthp = 0;
			if (boss.lefthp == 0) boss.survive = false;
			h->isDashing = false;             // ← 打中立即停止，不重复结算
			spawnEffect(h->locx, h->locy + 40, 2, 0);
			return;
		}

		spawnEffect(h->locx, h->locy + 40, 2, 0);

		if (h->dashTimer <= 0) h->isDashing = false;
		return;
	}

	// ======== 前摇期间锁定 ========
	if (h->castTimer > 0) return;

	static bool lastW = false;
	bool keyW = (GetAsyncKeyState('W') & 0x8000) != 0;
	bool keyA = (GetAsyncKeyState('A') & 0x8000) != 0;
	bool keyS = (GetAsyncKeyState('S') & 0x8000) != 0;
	bool keyD = (GetAsyncKeyState('D') & 0x8000) != 0;
	bool keyF = (GetAsyncKeyState('F') & 0x8000) != 0;

	// ======== 技能1加速Buff ========
	int spd = 5;
	if (h->speedBoostTimer > 0) {
		spd = 15;                // 三倍速
		h->speedBoostTimer--;
		// 加速中每帧生成残影特效
		if (h->speedBoostTimer % 3 == 0)
			spawnEffect(h->locx + 40, h->locy + 40, 1, 0);
	}

	// ======== 正常移动 ========
	if (keyA) {
		h->locx -= spd; lastad = 0;
		if (h->locx < LEFT_BOUND) h->locx = LEFT_BOUND;
	}
	if (keyD) {
		h->locx += spd; lastad = 1;
		if (h->locx > RIGHT_BOUND) h->locx = RIGHT_BOUND;
	}
	if (keyF) {
		h->locx += spd * 2; lastad = 1;
		if (h->locx > RIGHT_BOUND) h->locx = RIGHT_BOUND;
	}

	// 二段跳
	bool wPressed = keyW && !lastW;
	if (wPressed && h->jumpcount < 2) {
		h->velY = JUMP_POWER;
		h->jumpcount++;
		h->onground = false;
		h->iscrouching = false;
	}
	lastW = keyW;

	if (keyS && h->onground) h->iscrouching = true;
	else if (!keyS)          h->iscrouching = false;
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
		if(!mute)playBGM("battle.mp3");
		initgraph(1280, 750);                                      //奇犽：a 小杰 ：b
		IMAGE background;                                          //直立：a 跳跃：b 蹲下：c
		IMAGE bossImg;														   //左：a 右 b
		loadimage(&bossImg, _T("boss.bmp"));
		loadimage(&qysta.stand[0], _T("qy_left_resized.bmp"));
		loadimage(&qysta.stand[1], _T("qy_right_resized.bmp"));
		loadimage(&qysta.jump[0], _T("qy_jump_left_resized.bmp"));
		loadimage(&qysta.jump[1], _T("qy_jump_right_resized.bmp"));
		loadimage(&qysta.crouch[0], _T("qy_xd_left_resized.bmp"));
		loadimage(&qysta.crouch[1], _T("qy_xd_right_resized.bmp"));
		loadimage(&xjsta.stand[0], _T("xj_left_resized.bmp"));
		loadimage(&xjsta.stand[1], _T("xj_right_resized.bmp"));
		loadimage(&xjsta.jump[0], _T("xj_jump_left_resized.bmp"));
		loadimage(&xjsta.jump[1], _T("xj_jump_right_resized.bmp"));
		loadimage(&xjsta.crouch[0], _T("xj_xd_left_resized.bmp"));
		loadimage(&xjsta.crouch[1], _T("xj_xd_right_resized.bmp"));
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
			hero* ah = roles ? &xj : &qy;   // ah = active hero

			//tool();
			if (!gameWin && !gameLose) {
				gameFrames++;//计算时间
				move(ah);
				applyPhysics(ah);
				bossAI(ah);                      // bossAI 内调 handleAttack(h)，已修复
				IMAGE* t = getHeroFrame(ah);
				putImageTransparent(ah->locx, ah->locy, t);

				// 小杰专属绘制（在角色图之上）
				if (roles == 1) {
					drawXJCharge(ah);
					drawXJSword(ah);
				}
				updateAndDrawHeroBullets();

				drawBoss(&bossImg);
				drawEffects();
				drawHUD(ah);
				if (checkGameOver(ah) != 0) {
					// 游戏结束，停止更新逻辑
				}
			}
			// ---- 结算界面 ----
			if (gameWin) {
				drawBoss(&bossImg);
				showWinScreen(ah);
				if (GetAsyncKeyState('R') & 0x8000) {
					EndBatchDraw();
					Sleep(200);
					char nickname[22] = {};
					inputNickname(nickname);
					saveRank(finalScore, nickname);
					// 显示保存提示
					BeginBatchDraw();
					// ...提示文字...
					EndBatchDraw();
					Sleep(1500);
					break;
				}
				// 不按R就正常走到循环末尾的EndBatchDraw
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
		printf("game over!");
		if(!mute)stopBGM();
	}
	return;
}
int main()
{
	initmenu();
	itg();
	return 0;
}

