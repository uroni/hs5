import { FormEvent, Suspense, useState } from 'react';
import { router, state } from '../App';
import { Field } from "@fluentui/react-components";
import { Button, Input, Label, Spinner } from '@fluentui/react-components';
import { useQuery } from 'react-query';
import { DefaultService } from '../api/services';
import { useSnapshot } from 'valtio';
import { ApiError } from '../api';
import { HapiError, Herror } from '../errorapi/HapiError';

const Login = () => {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [isLoading, setLoading] = useState(false);
  const [usernameValidationMessage, setUsernameValidationMessage] = useState("");
  const [passwordValidationMessage, setPasswordValidationMessage] = useState("");

  const snap = useSnapshot(state);

  const handleSubmitInt = async (e : FormEvent<HTMLFormElement>) => {

    if(!username)
    {
      setUsernameValidationMessage("Username is empty");
      return;
    }
    
    try
    {
      const loginRes = await DefaultService.postApiV1B64Be5124B034028A58913931942E205Login({requestBody: {username: username, password: password}});
      state.session = loginRes.ses;
    }
    catch(apiE)
    {
      if(apiE instanceof ApiError)
      {
        const e = apiE.body as HapiError;
        switch(e.herror)
        {
          case Herror.UserNotFound:
            setUsernameValidationMessage("User not found on server");
            break;
          case Herror.PasswordWrong:
            setUsernameValidationMessage("Login with username and password combination failed");
            setPasswordValidationMessage("Login with username and password combination failed");
            break;
          default:
            // TODO: Show error message screen
        }
      }
      else
      {
        throw apiE;
      }
    }
  };

  const handleSubmit = async (e : FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    setLoading(true);
    try
    {
      await handleSubmitInt(e);
    }
    finally
    {
      setLoading(false);
    }
  }

  return (
    <div style={{ display: "flex",
        alignItems: "center",
        justifyContent: "center",
        height: "100%"
      }}>
    <Suspense fallback={<Spinner/>}>
      <div>
        <h3>Login:</h3>
        <div>
          <form onSubmit={handleSubmit}>
            <Field
              label="Username" required validationMessage={usernameValidationMessage}>
                <Input  id='username' value={username}  onChange={(e) => { setUsername(e.target.value)}}/>
              </Field>
              <Field
              label="Password" required validationMessage={passwordValidationMessage}>
                <Input id='password' type='password' value={password} onChange={(e) => { setPassword(e.target.value)}}/>
              </Field>
              {isLoading && <Spinner label="Logging in..." />}
              {!isLoading && <Button type="submit">Log in</Button>}
          </form>
        </div>
      </div>
    </Suspense>
    </div>
  );
};

export default Login;
