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
import { QueryClient, QueryClientProvider } from 'react-query';

const initialDark = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
const initialTheme =
  initialDark ? teamsDarkTheme : teamsLightTheme;

export enum Pages {
  Buckets = "buckets",
  Login = "login",
  About = "about"
}

export const state = proxy({
  loggedIn: false,
  activePage: Pages.Buckets,
  session: ""
});

function getSessionFromLocalStorage() : string
{
  if(!window.localStorage)
    return "";
  return localStorage.getItem("ses") ?? "";
}

export function saveSessionToLocalStorage(session: string)
{
  if(!window.localStorage)
    return;
  localStorage.setItem("ses", session);
}

export const router = createHashRouter([
  {
    path: `/`,
    element: (
      <LoginPage />
    ),
    loader: async () => {
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
      state.activePage = Pages.Buckets;
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
