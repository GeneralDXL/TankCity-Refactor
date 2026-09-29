const net = require('net');
const http = require('http');
const url = require('url');
const uuid = require('uuid');

// 配置参数
const TCP_HOST = '127.0.0.1';
const TCP_PORT = 12345;
const HTTP_PORT = 8080;
const MAX_MESSAGES = 5000;

// 数据结构
const messageStore = [];
let messageCount = 0;
let gameState = {
  players: {},
  enemies: [],
  bullets: [],
  walls: {},
  gameRunning: false,
  score: 0
};

// 消息类型API定义
const apiDefinitions = {
  player_init: "玩家加入游戏",
  player_left: "玩家离开游戏",
  player_died: "玩家死亡",
  enemy_init: "敌人创建",
  enemy_died: "敌人死亡",
  bullet: "子弹状态更新",
  bullet_created: "子弹创建",
  delete_wall: "墙壁删除",
  game_start: "游戏开始",
  map_init: "地图初始化",
  game_over: "游戏结束",
  game_state: "完整游戏状态"
};

class GameClient {
  constructor() {
    this.buffer = Buffer.alloc(0);
    this.expectedLength = 0;
    this.client = new net.Socket();
  }

  connect() {
    this.client.connect(TCP_PORT, TCP_HOST, () => {
      console.log(`[TCP] Connected to ${TCP_HOST}:${TCP_PORT}`);
    });

    this.client.on('data', (data) => this.handleData(data));
    this.client.on('close', () => console.log('[TCP] Connection closed'));
    this.client.on('error', (err) => console.error('[TCP] Connection error:', err));
  }

  disconnect() {
    this.client.end();
    console.log('[TCP] Disconnected by user');
  }

  handleData(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    
    while (this.buffer.length >= 4 || this.expectedLength > 0) {
      if (this.expectedLength === 0) {
        this.expectedLength = this.buffer.readUInt32BE(0);
        this.buffer = this.buffer.slice(4);
      }
      
      if (this.buffer.length >= this.expectedLength) {
        const message = this.buffer.slice(0, this.expectedLength);
        this.buffer = this.buffer.slice(this.expectedLength);
        this.expectedLength = 0;
        
        this.processMessage(message);
      } else {
        break;
      }
    }
  }

  processMessage(message) {
    try {
      const json = JSON.parse(message.toString('utf8'));
      messageCount++;
      
      // 添加元数据
      const enhancedMessage = {
        id: uuid.v4(),
        timestamp: new Date().toISOString(),
        type: json.type,
        data: json
      };
      
      // 存储消息
      if (messageStore.length >= MAX_MESSAGES) messageStore.shift();
      messageStore.push(enhancedMessage);
      
      // 更新游戏状态
      this.updateGameState(enhancedMessage);
      
      // 特殊事件处理
      if (json.type === 'game_over') {
        console.log('[GAME] Game Over event detected');
      }
    } catch (e) {
      console.error('Message parse error:', e.message);
    }
  }

  updateGameState(msg) {
    const { type, data } = msg;
    
    switch(type) {
      case 'player_init':
        gameState.players[data.id] = {
          position: data.position,
          bodyAngle: data.bodyAngle,
          turretAngle: data.turretAngle,
          health: data.health
        };
        break;
        
      case 'player_left':
      case 'player_died':
        if (gameState.players[data.id]) {
          delete gameState.players[data.id];
        }
        break;
        
      case 'enemy_init':
        gameState.enemies.push({
          id: data.id,
          position: data.position,
          bodyAngle: data.bodyAngle,
          turretAngle: data.turretAngle,
          health: data.health,
          difficulty: data.difficulty
        });
        break;
        
      case 'enemy_died':
        gameState.enemies = gameState.enemies.filter(e => e.id !== data.id);
        break;
        
      case 'bullet_created':
        gameState.bullets.push({
          position: data.position,
          angle: data.angle,
          type: data.type1
        });
        break;
        
      case 'bullet':
        gameState.bullets = data.bullets.map(b => ({
          position: b.position,
          angle: b.angle,
          type: b.type
        }));
        break;
        
      case 'delete_wall':
        if (gameState.walls[data.wallId]) {
          delete gameState.walls[data.wallId];
        }
        break;
        
      case 'game_start':
        gameState.players = {};
        gameState.enemies = [];
        gameState.bullets = [];
        gameState.walls = {};
        gameState.gameRunning = true;
        gameState.score = 0;
        break;
        
      case 'map_init':
        gameState.walls = {};
        data.walls.forEach(wall => {
          gameState.walls[wall.id] = {
            position: wall.position,
            size: wall.size,
            type: wall.type
          };
        });
        break;
        
      case 'game_over':
        gameState.gameRunning = false;
        break;
        
      case 'game_state':
        // 更新玩家
        data.players.forEach(p => {
          gameState.players[p.id] = {
            position: p.position,
            bodyAngle: p.bodyAngle,
            turretAngle: p.turretAngle,
            health: p.health
          };
        });
        
        // 更新敌人
        gameState.enemies = data.enemies.map(e => ({
          id: e.id,
          position: e.position,
          bodyAngle: e.bodyAngle,
          turretAngle: e.turretAngle,
          health: e.health,
          difficulty: e.difficulty
        }));
        
        // 更新子弹
        gameState.bullets = data.bullets.map(b => ({
          position: b.position,
          angle: b.angle,
          type: b.type1
        }));
        
        // 更新分数
        gameState.score = data.score;
        break;
    }
  }
}

const gameClient = new GameClient();
gameClient.connect();

const httpServer = http.createServer((req, res) => {
  const parsedUrl = url.parse(req.url, true);
  const path = parsedUrl.pathname;
  
  // 设置CORS头
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, GET, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  
  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    res.end();
    return;
  }
  
  // API路由
  if (req.method === 'GET') {
    if (path === '/status') {
      handleStatusRequest(res);
    } else if (path === '/types') {
      handleTypesRequest(res);
    } else if (Object.keys(apiDefinitions).some(type => path === `/${type}`)) {
      handleMessageTypeRequest(path, parsedUrl, res);
    } else {
      res.writeHead(404, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Not found' }));
    }
  } else if (req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk.toString());
    req.on('end', () => {
      try {
        const requestData = body ? JSON.parse(body) : {};
        
        if (path === '/query') {
          handleQueryRequest(requestData, res);
        } else if (path === '/control') {
          handleControlRequest(requestData, res);
        } else {
          res.writeHead(404, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ error: 'Endpoint not found' }));
        }
      } catch (e) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Invalid JSON format' }));
      }
    });
  } else {
    res.writeHead(405, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Method not allowed' }));
  }
});

// 启动HTTP服务器
httpServer.listen(HTTP_PORT, () => {
  console.log(`[HTTP] API server running at http://localhost:${HTTP_PORT}/`);
  console.log(`[API] Available endpoints:`);
  Object.keys(apiDefinitions).forEach(type => {
    console.log(`[API] GET /${type} - ${apiDefinitions[type]}`);
  });
});

// 处理状态请求
function handleStatusRequest(res) {
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({
    status: 'ok',
    gameRunning: gameState.gameRunning,
    score: gameState.score,
    playersCount: Object.keys(gameState.players).length,
    enemiesCount: gameState.enemies.length,
    bulletsCount: gameState.bullets.length,
    wallsCount: Object.keys(gameState.walls).length
  }));
}

// 处理消息类型请求
function handleMessageTypeRequest(path, parsedUrl, res) {
  const type = path.substring(1); // 去掉开头的斜杠
  const limit = parseInt(parsedUrl.query.limit) || 5;
  
  // 过滤指定类型的消息
  const results = messageStore.filter(msg => msg.type === type)
                              .slice(-limit)
                              .reverse(); // 最新的排在最前
  
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({
    type: type,
    description: apiDefinitions[type],
    count: results.length,
    results: results
  }));
}

// 处理查询请求
function handleQueryRequest(data, res) {
  let results = messageStore;
  
  // 应用过滤器
  if (data.filters) {
    results = results.filter(msg => {
      for (const key in data.filters) {
        if (!msg.data[key] || msg.data[key] !== data.filters[key]) {
          return false;
        }
      }
      return true;
    });
  }
  
  // 限制结果数量
  if (data.limit > 0) {
    results = results.slice(0, data.limit);
  }
  
  // 排序
  if (data.sort) {
    if (data.sort === 'desc') {
      results.reverse();
    }
  }
  
  // 聚合统计
  const typeStats = {};
  results.forEach(msg => {
    if (!typeStats[msg.type]) typeStats[msg.type] = 0;
    typeStats[msg.type]++;
  });
  
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({
    status: 'ok',
    total: results.length,
    stats: typeStats,
    results: results
  }));
}

// 处理控制请求
function handleControlRequest(data, res) {
  const response = { status: 'ok' };
  
  switch (data.command) {
    case 'start':
      gameClient.connect();
      response.message = 'TCP connection started';
      break;
      
    case 'stop':
      gameClient.disconnect();
      response.message = 'TCP connection stopped';
      break;
      
    case 'reset':
      messageStore.length = 0;
      messageCount = 0;
      response.message = 'Message store reset';
      break;
      
    default:
      response.status = 'error';
      response.error = 'Unknown command';
      res.writeHead(400, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(response));
      return;
  }
  
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify(response));
}

// 处理可用类型请求
function handleTypesRequest(res) {
  const types = {};
  
  for (const type of Object.keys(apiDefinitions)) {
    types[type] = {
      description: apiDefinitions[type],
      count: messageStore.filter(msg => msg.type === type).length
    };
  }
  
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({
    status: 'ok',
    types: types,
    totalEvents: messageCount
  }));
}

// 优雅关闭
process.on('SIGINT', () => {
  console.log('Shutting down...');
  gameClient.disconnect();
  httpServer.close();
  process.exit();
});
