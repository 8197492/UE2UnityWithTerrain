using UnityEngine;
using VenusRendering;

public class TestCall : MonoBehaviour
{
    private void Awake()
    {
        Utility.Init();
    }

    private void OnDestroy()
    {
        Utility.Term();
    }

	void Update ()
    {
        //int val = Utility.GetTestInteger();
        //Debug.Log(val);
        //GetComponent<Camera>().transform.Rotate(Vector3.right, (float)val * 0.1f);
    }
}
