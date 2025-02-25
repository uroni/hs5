import * as React from 'react';
import { useEffect, useState } from 'react';
import HeaderBar from './components/HeaderBar';
import NavSidebar from './components/NavSidebar';
import { proxy, useSnapshot } from 'valtio';
import { createHashRouter, RouterProvider } from 'react-router-dom';
import LoginPage from './pages/Login';
import Buckets from './pages/Buckets';
import { FluentProvider, teamsLightTheme, teamsDarkTheme, makeStyles } from '@fluentui/react-components';
import { useStackStyles } from './components/StackStyles';
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { ApiError, postApiV1B64Be5124B034028A58913931942E205SessionCheck } from './api';
import { HapiError, Herror } from './errorapi/HapiError';
import "./css/global.css";
import AddBucket from './pages/AddBucket';

const initialDark = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
const initialTheme =
  initialDark ? teamsDarkTheme : teamsLightTheme;

export enum Pages {
  Buckets = "buckets",
  Login = "login",
  About = "about",
  AddBucket = "AddBucket"
}

export const state = proxy({
  loggedIn: false,
  activePage: Pages.Buckets,
  pageAfterLogin: Pages.Buckets,
  session: "",
  startupComplete: false,
  accessKey: "",
  secretAccessKey: ""
});

function getSessionFromLocalStorage() : void
{
  if(!window.localStorage)
    return;
  state.session = localStorage.getItem("ses") ?? "";
  state.accessKey = localStorage.getItem("accessKey") ?? "";
  state.secretAccessKey = localStorage.getItem("secretAccessKey") ?? "";
}

async function isLoggedIn(): Promise<boolean> {
  try {
    if(!state.session)
      getSessionFromLocalStorage();
    await postApiV1B64Be5124B034028A58913931942E205SessionCheck({requestBody: {ses: state.session}});
  } catch (error) {
    if (error instanceof ApiError)
    {
      const e = error.body as HapiError;
      switch(e.herror)
      {
        case Herror.SessionNotFound:
        case Herror.SessionRequired:
          return false;
      }
    }
    throw error;
  }
  return true;
}

async function jumpToLoginPageIfNeccessary() {
  if (state.startupComplete && state.loggedIn) {
    state.activePage = state.pageAfterLogin;
    return;
  }

  if (await isLoggedIn()) {
    state.loggedIn = true;
    state.startupComplete = true;
    state.activePage = state.pageAfterLogin;
  } else {
    state.loggedIn = false;
    await router.navigate(`/`);
  }
}

export function saveSessionToLocalStorage()
{
  if(!window.localStorage)
    return;
  localStorage.setItem("ses", state.session);
  localStorage.setItem("accessKey", state.accessKey);
  localStorage.setItem("secretAccessKey", state.secretAccessKey);
}

export const router = createHashRouter([
  {
    path: `/`,
    element: (
      <LoginPage />
    ),
    loader: async () => {
      if (await isLoggedIn()) {
        state.loggedIn = true;
        state.startupComplete = true;
        await router.navigate(`/${Pages.Buckets}`);
        return;
      }
      state.startupComplete = true;
      state.loggedIn = false;
      state.activePage = Pages.Login;
      return null;
    }
  },
  {
    path: `/${Pages.Buckets}`,
    element: (
      <Buckets />
    ),
    loader: async () => {
      state.pageAfterLogin = Pages.Buckets;
      await jumpToLoginPageIfNeccessary();
      return null;
    }
  },
  {
    path: `/${Pages.AddBucket}`,
    element: (
      <AddBucket />
    ),
    loader: async () => {
      state.pageAfterLogin = Pages.AddBucket;
      await jumpToLoginPageIfNeccessary();
      return null;
    }
  },
  {
    path: `/about`,
    element: <div>About page</div>
  }
]);

const queryClient = new QueryClient();

const App: React.FunctionComponent = () => {
  const [selectedTheme, setTheme] = useState(initialTheme);

  const snap = useSnapshot(state);

  useEffect(() => {
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', event => {
      setTheme(event.matches ? teamsDarkTheme : teamsLightTheme);
    });
  }, []);

  const styles = useStackStyles();

  return (
      <FluentProvider theme={selectedTheme} style={{ height: "100%" }}>
        <React.StrictMode>
          <QueryClientProvider client={queryClient}>
            <div className={styles.stackVertical}>
              <div className={styles.item}>
                  <HeaderBar />
                </div>
                <div className={styles.itemGrow}>
                  <div className={styles.stackHorizontal}>
                    {snap.loggedIn &&
                      <div className={styles.item} style={{ borderRight: "1px solid", padding: "10pt" }}>
                        <NavSidebar />
                      </div>
                    }
                    <div className={styles.itemGrow} style={{padding: "10pt"}}>
                      <RouterProvider router={router} />
                    </div>
                </div>
              </div>
            </div>
          </QueryClientProvider>
        </React.StrictMode>
      </FluentProvider>
  );
};

export default App;
