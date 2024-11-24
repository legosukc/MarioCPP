#define SDL_MAIN_HANDLED // Tell SDL not to mess with main() (stupid motherfucking bitch ass)

#define DEBUG false

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <water.h>

using namespace std;

// Config
#define GlobalScale 4u // Scales the size of everything (resolution, sprite sizes, etc)
#define MoveScale 1 // Scales everything movement related
#define Gravity 0.280f // The default gravity
#define ScrW 256u * GlobalScale
#define ScrH 240u * GlobalScale
#define WaterLevel 0 // The height of the water, set to screen height to disable.

// Mario Config
#define WalkAccel 0.152f * MoveScale //0.152f
#define RunAccel 0.228f * MoveScale //0.228f

#define MaxWalkSpeed 1.900f * MoveScale //1.900f
#define MaxSwimWalkSpeed 1.100f * MoveScale // 1.100f
#define MaxRunSpeed 2.900f * MoveScale //2.900f
#define MinimumSpeed 0.130f * MoveScale //0.130f

#define ReleaseDeccel 0.130f * MoveScale //0.130f
#define SkidDecel 0.200f * MoveScale

#define SkidTurnaroundSpeed 0.900f * MoveScale // If mario is at or below this speed while skidding, he will stop and turn around.
//#define 0.110f

// Mario Variables
bool mJump = false, // Jump Flag
mGrounded = false, // Touching Ground Flag
mJumpGravityReduction = false, // Flag that keeps track of jump held
mJumpFlag = false, // Makes sure that the player can't just hold the jump button to keep jumping.
mSwim = false,
mSwimUpAnim = false,
mAirCap = false; // Flag to decide which speed cap to use in the air

int8_t mDir = 1; // The direction mario is facing (1 = right, -1 = left)
Uint8 mRun = 0u; // Keeps track if the player is running, also used as the 10-frame delay from running to walking

float mFrame = 0.f, // The animation frame
mInitJumpVX = 0.f, // The velocity the player jumped at.
mVX = 0.f, // Horizontal (X) Velocity
mVY = 0.f, // Vertical (Y) Velocity
mGravity = Gravity, // Gravity exclusive for the player
mGravityAfterRelease = Gravity, // The value to revert to when the player stops holding jump
mX = 0.f, // Real rect position (so we dont have to make every collideable object (rect) a float.)
mY = 0.f;

__forceinline float clamp(float n, float maximum) {
    return (n < -maximum) ? -maximum : (n < maximum ? n : maximum);
}

__forceinline bool WithinRange(float n, float lower, float higher, bool CanEqual = true) {
    if (CanEqual) return (n <= higher && n >= lower);
    else return (n < higher && n > lower);
}

bool Collision(SDL_Rect* rect, float& vel, float& Fpos, int& Rpos, const SDL_Rect* floor) {
    Fpos += vel * GlobalScale;
    Rpos = static_cast<int>(Fpos);
    if (SDL_HasIntersection(rect, floor)) {
        const int8_t velDir = (vel > 0.f) ? -1 : 1;
        while (SDL_HasIntersection(rect, floor)) {
            Rpos += velDir;
            Fpos = static_cast<float>(Rpos);
        }
        return true;
    }
    return false;
}

bool _fastcall mUpdateAnimation(float Speed, Uint8 Cap, Uint8 Offset, SDL_Rect*& curFrame) {
    mFrame += Speed;
    if (mFrame >= Cap) mFrame = 0.f;
    curFrame->x = (16 * static_cast<int>(mFrame)) + Offset;
    return mFrame == 0.f;
}

__forceinline void mUpdate(SDL_Rect* rect, const SDL_Rect* floor, SDL_Rect*& curFrame) {
    const Uint8* Input = SDL_GetKeyboardState(NULL);
    // Running
    if (Input[SDL_SCANCODE_LSHIFT]) {
        mRun = 10u;
    } else if (mRun != 0u) {
        mRun--;
    }

    // Movement
    const int8_t moveX = Input[SDL_SCANCODE_D] - Input[SDL_SCANCODE_A];
    if (mGrounded) { // Ground Movement
        if (moveX == 0) {
            if (mVX != 0.f) {
                mVX -= (mVX > 0.f) ? ReleaseDeccel : -ReleaseDeccel;
            }
        } else { // Moving
            if (mRun != 0u && !mSwim) {
                mVX = clamp(mVX + moveX * RunAccel, MaxRunSpeed);
            } else {
                mVX = clamp(mVX + moveX * WalkAccel, (mSwim) ? MaxSwimWalkSpeed : MaxWalkSpeed);
            }

            mAirCap = SDL_fabsf(mVX) >= MaxWalkSpeed;
            mDir = moveX;
        }
        if (SDL_fabsf(mVX) < MinimumSpeed) {
            mVX = 0.f;
        }

        // Jump
        if (!mSwim) {
            if (Input[SDL_SCANCODE_SPACE]) {
                if (mJump && !mJumpFlag) {
                    mJumpFlag = true;
                    mJumpGravityReduction = true;
                    mJump = false;
                    mInitJumpVX = SDL_fabsf(mVX);

                    mVY = -5.f; //-4
                    mGravity = 0.2f;
                    if (mInitJumpVX < MoveScale) {
                        mGravityAfterRelease = 0.7f;
                    } else if (WithinRange(mInitJumpVX, MoveScale, 2.565f * MoveScale)) { // 2.565f
                        mGravityAfterRelease = 0.6f;
                    } else if (mInitJumpVX >= 2.5f * MoveScale) {
                        mVY = -6.f; //-5
                        mGravityAfterRelease = 0.9f;
                    }
                    mVY *= MoveScale;
                    mGravity *= MoveScale;
                    mGravityAfterRelease *= MoveScale;
                }
            } else {
                mJumpFlag = false;
            }
        }
    } else {
        if (mSwim) {
            mVX = clamp(mVX + moveX * WalkAccel, MaxWalkSpeed);
        } else {
            mVX = clamp(mVX + moveX * ((moveX == mDir) ? (mAirCap ? WalkAccel : RunAccel) : 0.1f), (mInitJumpVX < MaxWalkSpeed) ? MaxWalkSpeed : MaxRunSpeed); // 0.152f 0.228f
        }

        // Jump Gravity Shtuff
        if (mJumpGravityReduction && (mVY >= 0.f or !Input[SDL_SCANCODE_SPACE])) {
            mJumpGravityReduction = false;
            mGravity = mGravityAfterRelease;
        }
    }

    // Swim
    if (mSwim) {
        if (moveX != 0) mDir = moveX;

        if (Input[SDL_SCANCODE_SPACE]) {
            if (!mJumpFlag) {
                mJumpFlag = true;
                mJumpGravityReduction = true;
                mSwimUpAnim = true;
                mFrame = 0.f;
                mInitJumpVX = SDL_fabsf(mVX);

                mVY = -1.8f * MoveScale;
            }
            mGravity = 0.13f;
        } else {
            mGravity = 0.1f;
            mJumpFlag = false;
        }
        mGravity *= MoveScale;
    }
    // X Collision
    if (Collision(rect, mVX, mX, rect->x, floor)) {
        mVX = 0.f;
    }

    // Y Collision
    if (Collision(rect, mVY, mY, rect->y, floor)) {
        if (mVY >= 0.f) {
            mGravity = Gravity;
            mGravityAfterRelease = Gravity;
        } else {
            mGrounded = false;
            mJump = false;
        }
        mVY = 0.f;
    } else {
        rect->y++;
        mGrounded = SDL_HasIntersection(rect, floor);
        if (mGrounded) {
            mVY = 0.f;
        } else {
            mVY += mGravity;
        }
        rect->y--;
    }

    mSwim = (rect->y > WaterLevel);

    // Texture
    if (mGrounded) {
        curFrame->y = 0;
        if (mVX == 0.f) { // mIdle
            mFrame = 0.f;
            curFrame->x = 0;
        } else { // mRun
            mUpdateAnimation((SDL_fabsf(mVX) / MaxRunSpeed) * 0.5f, 3u, 32u, curFrame);
        }
    } else if (mSwim) {
        curFrame->y = 16;
        if (mSwimUpAnim) {
            if (mUpdateAnimation(0.25f, 4, 48, curFrame)) {
                mSwimUpAnim = false;
            }
        } else {
            mUpdateAnimation((SDL_fabsf(mVX) + SDL_fabsf(mVY)) * 0.1f / MaxWalkSpeed, 2u, 32u, curFrame);
        }
    } else if (!mJump) { // mJump
        mFrame = 0.f;
        curFrame->x = 16;
        curFrame->y = 0;
    }
}

int _cdecl main() {
    // Initialize SDL
#if DEBUG
    string success = SDL_Init(SDL_INIT_VIDEO) != 0 ? "SDL_Init Error" : "";
#else
    bool success = SDL_Init(SDL_INIT_VIDEO) == 0;
#endif

    // Create an SDL window that supports OpenGL rendering.
    SDL_Window* window = SDL_CreateWindow("SDL Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256u * GlobalScale, 240u * GlobalScale, SDL_WINDOW_OPENGL);
    if (window == NULL) {
#if DEBUG
        success = "SDL_CreateWindow Error";
#else
        success = false;
#endif
    }

    // Create the renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
#if DEBUG
        success = "SDL_CreateRenderer Error";
#else
        success = false;
#endif
    }

    const SDL_Rect* floor = new SDL_Rect{ 0, 230 * GlobalScale, 256 * GlobalScale, 20 * GlobalScale };

    SDL_Rect* mRect = new SDL_Rect{ 0, 0, 16 * GlobalScale, 16 * GlobalScale }; // Player hitbox

    // Make black pixels transparent and convert to texture.
    SDL_Surface* mSheets = IMG_Load("spr/mSheet.png");
    SDL_SetColorKey(mSheets, SDL_TRUE, SDL_MapRGBA(mSheets->format, 0, 0, 0, 0));
    SDL_Texture* mSheet = SDL_CreateTextureFromSurface(renderer, mSheets); // Sprite sheet containing the players animations.
    SDL_FreeSurface(mSheets);

    //SDL_Palette* mFire;
    //SDL_SetPaletteColors(mFire, )

    SDL_Rect* mCurrentFrame = new SDL_Rect{ 0, 0, 16, 16 }; // The rect that displays what part of mSheet to render.

#if DEBUG
    bool running = success == "";
#else
    bool running = success;
#endif
    SDL_Event event;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
        // Update
        mUpdate(mRect, floor, mCurrentFrame);

        // Draw
        // Set the draw color to black and clear window
        SDL_SetRenderDrawColor(renderer, 50u, 50u, 50u, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255u, 255u, 255u, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, floor);

        SDL_RenderCopyEx(renderer, mSheet, mCurrentFrame, mRect, 0, 0, (mDir == 1) ? SDL_RendererFlip::SDL_FLIP_NONE : SDL_RendererFlip::SDL_FLIP_HORIZONTAL);

        WaterRenderer(renderer, 256u * GlobalScale, 240u * GlobalScale);

        SDL_RenderPresent(renderer);

        SDL_Delay(16u);
    }
    // Clean up
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
#if DEBUG
    cerr << success + ": " + static_cast<string>(SDL_GetError()) << endl;
#endif
    SDL_Quit();
    return 0;
}