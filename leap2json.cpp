#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <Leap.h>

/* Target IP and port. */
#define TARGET_IP "127.0.0.1"
#define TARGET_PORT 50507

/* How often to send the data.
 * 16 = ~62fps
 * 33 = ~30fps
 * 50 = 20fps
 * */
#define FREQUENCY 16

std::string matrix_to_quads(Leap::Matrix m, int is_right)
{
    /* Me trying to understand the matrix.
     *
     * 0 1 2
     * 3 4 5
     * 6 7 8
     * */

    Leap::FloatArray a = m.toArray3x3();
    float t1, t2, t3;

    /* Neutral hand position matrix. Used for testing stuff.
    a[0] = 1;
    a[1] = 0;
    a[2] = 0;
    a[3] = 0;
    a[4] = 1;
    a[5] = 0;
    a[6] = 0;
    a[7] = 0;
    a[8] = 1;
    */

    /* Rotate 90 
     * Hands are pointing 90 degrees in the wrong direction sometimes so I need to compensate for it.
    t1 = a[0];
    t2 = a[3];
    t3 = a[6];
    a[0] = a[1] * -1;
    a[3] = a[4] * -1;
    2a[6] = a[7] * -1;
    a[1] = t1;
    a[4] = t2;
    a[7] = t3;
    */
    
    /* Rotate 180 
     * Right hand will be rotated the wrong direction so we want to flip it 180 degrees.
    if (is_right)
    {
        a[0] = a[0] * -1;
        a[3] = a[3] * -1;
        a[6] = a[6] * -1;
        a[1] = a[1] * -1;
        a[4] = a[4] * -1;
        a[7] = a[7] * -1;
    }
     */
    
    /* Some formula to make quaternions. I don't know either matrices or quaternions so I tried to learn it.
     * https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
     * */
    float w = sqrt(1.0 + a[0] + a[4] + a[8]) / 2.0;

    if (!w || std::isnan(w))
        return "{}";

    float x = (a[7] - a[5]) / (4 * w);
    float y = (a[2] - a[6]) / (4 * w);
    float z = (a[3] - a[1]) / (4 * w);

    /* Packed into nice json. */
    return "{"
            "\"qw\":" + std::to_string(w) + "," +
            "\"qx\":" + std::to_string(x) + "," +
            "\"qy\":" + std::to_string(y) + "," +
            "\"qz\":" + std::to_string(z) + "}";
}

std::string getFinger(Leap::Finger finger, int is_right)
{
    std::string f;
    
    /* Go through all bones and get the quaternions for each. */
    Leap::Bone b = finger.bone(Leap::Bone::Type::TYPE_PROXIMAL);
    f = "{\"Proximal\":" + matrix_to_quads(b.basis(), is_right) + ",";

    b = finger.bone(Leap::Bone::Type::TYPE_INTERMEDIATE);
    f += "\"Intermediate\":" + matrix_to_quads(b.basis(), is_right) + ",";

    b = finger.bone(Leap::Bone::Type::TYPE_DISTAL);
    f += "\"Distal\":" + matrix_to_quads(b.basis(), is_right) + "}";

    return f;
}

std::string getHand(Leap::Hand hand)
{
    int is_right = 0;
    if (hand.isRight())
        is_right = 1;

    std::string h = "{\"palm\":" + matrix_to_quads(hand.basis(), is_right) + "," +
        "\"arm\":{" +
        "\"x\":" + std::to_string(hand.arm().wristPosition().x) + "," +
        "\"y\":" + std::to_string(hand.arm().wristPosition().y) + "," +
        "\"z\":" + std::to_string(hand.arm().wristPosition().z) + "," +
        "\"rotation\":" + matrix_to_quads(hand.arm().basis(), is_right) + "}";
    
    /* Go through all the fingers and make json. */
    Leap::FingerList fingers = hand.fingers();
    for(Leap::FingerList::const_iterator fl = fingers.begin(); fl != fingers.end(); fl++)
    {
        switch ((*fl).type())
        {
        case Leap::Finger::TYPE_PINKY:
            h += ",\"Little\":" + getFinger((*fl), is_right);
            break;

        case Leap::Finger::TYPE_RING:
            h += ",\"Ring\":" + getFinger((*fl), is_right);
            break;

        case Leap::Finger::TYPE_MIDDLE:
            h += ",\"Middle\":" + getFinger((*fl), is_right);
            break;

        case Leap::Finger::TYPE_INDEX:
            h += ",\"Index\":" + getFinger((*fl), is_right);
            break;

        case Leap::Finger::TYPE_THUMB:
            h += ",\"Thumb\":" + getFinger((*fl), is_right);
            break;
        }
    }

    h += "}";

    return h;
}

int main(void)
{
    /* Get Leap Motion connection */
    Leap::Controller leap_c;

    std::cout << "Waiting for Leap." << std::endl;

    while (!leap_c.isConnected())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Connected." << std::endl;

    /* Port and socket. */
    int port = TARGET_PORT;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        std::cout << "Failed to create socket." << std::endl;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(TARGET_IP);
    
    while (1)
    {
        char buffer[4096];

        Leap::Hand right, left;

        while (1)
        {
            /* Get Leap data and make response. */
            const Leap::Frame f = leap_c.frame();
            if (f.hands().rightmost().isRight())
                right = f.hands().rightmost();
            if (f.hands().leftmost().isLeft())
                left = f.hands().leftmost();
            
            /* Make the hand in json */
            std::string m = "{\"Right\":" + getHand(right) + 
                ",\"Left\":" + getHand(left) + 
                "}";
            
            /* Send the data */
            int b_sent = sendto(sock, m.c_str(), m.length(), 0,
                    (struct sockaddr*) &addr, sizeof(addr));
            if (b_sent < 0)
            {
                std::cout << "Failed to send data." << std::endl;
                return 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(FREQUENCY));
        }
    }

    return 0;
}
